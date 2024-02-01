#!/usr/bin/env python3

import sys
import os
from datetime import datetime
import errno
import logging  
import re
import select
import signal
import socket
from string import Template
import struct
import subprocess
import threading
import time
from argparse import ArgumentParser

from process_utils import get_process_id_by_name

try:
    import colorlog
    handler = colorlog.StreamHandler()
    handler.setFormatter(colorlog.ColoredFormatter('%(log_color)sPYTHON %(asctime)s %(levelname)s: %(message)s'))

    logger = colorlog.getLogger(__file__)
    logger.addHandler(handler)
except Exception as e:
    print(e)
    logger = logging.getLogger(__file__)
    logging.basicConfig(format='PYTHON %(asctime)s.%(msecs)03d %(levelname)s: %(message)s', level=logging.DEBUG, datefmt='%H:%M:%S')

# 7-bit and 8-bit C1 ANSI sequences
ansi_escape_8bit = re.compile(br'''
    (?: # either 7-bit C1, two bytes, ESC Fe (omitting CSI)
        \x1B
        [@-Z\\-_]
    |   # or a single 8-bit byte Fe (omitting CSI)
        [\x80-\x9A\x9C-\x9F]
    |   # or CSI + control codes
        (?: # 7-bit CSI, ESC [ 
            \x1B\[
        |   # 8-bit CSI, 9B
            \x9B
        )
        [0-?]*  # Parameter bytes
        [ -/]*  # Intermediate bytes
        [@-~]   # Final byte
    )
''', re.VERBOSE)


class CustomTemplate(Template):
	delimiter = '%$%'


HOST = "127.0.0.1"  # The server's hostname or IP address
UW_APP_TCP_PORT_BASE = 4000
NUM_SEND_NODES = 2
UW_APP_UDP_POS_PORT_BASE = 5000

last_send_dt = None


class RepeatTimer(threading.Timer):  
    def run(self):
        while not self.finished.wait(self.interval):  
            self.function(*self.args,**self.kwargs)  


class SingleNode(object):
    def __init__(self, id: int, send_interval: float) -> None:
        """
        id: node id
        send_interval: send interval in [s], set to 0.0 to not send messages
        """
        self.node_id = id
        self.send_interval = send_interval
        self.last_send_dt = None


def _recv_wait(s: socket.socket, siz: int) -> bytes:
    """Receive siz data bytes for non-blocking socket"""
    d = bytes()
    while len(d) < siz:
        try:
            d += s.recv(siz-len(d))
        except socket.error as e:
            if e.errno != errno.EAGAIN:
                raise e
    return d

def _recv_data(sock_list) -> tuple[bool, bytes]:
    """Receive AcoSim message with header"""
    for s in sock_list:
        header = s.recv(2)            
        if header == b'':
            return False, b""
        if header[0] == 68: # 'D'
            # data = s.recv(header[1])                         
            data = _recv_wait(s, header[1])
            if data:
                # A readable client socket has data
                return True, data
            else:
                # Interpret empty result as closed connection
                return False, b""
    return True, b""

def _send_data_nonblocking(s: socket.socket, id:int, msg: bytes) -> int:
    global last_send_dt
    total_sent = 0
    # data = struct.pack("<cB", b'D', len(msg)) + msg
    data = msg    
    while len(data):
        try:
            sent = s.send(data)
            total_sent += sent
            data = data[sent:]
        except socket.error as e:
            if e.errno != errno.EAGAIN:
                raise e
            return -total_sent
    last_send_dt = datetime.now()
    logger.info(f"Node {id} sent message '{msg.decode()}'")
    return total_sent

def connect_socket(s: socket.socket, address: tuple, retries: int = 5) -> bool:
    counter = 0
    while counter < retries:
        try:
            s.connect(address)
            return True  
        except socket.error as error:
            print(f"Connection ot {address} failed, reason: {error}")
            print(f"Attempt {counter} of {retries}")
            counter += 1
        time.sleep(1.0)
    return False

def recv_send_worker(id: int, send_interval: int):
    """ Worker thread, receives (and optionally sends) messages to ns2.
        Sending is only active when send_interval > 0.0
    """
    global last_send_dt
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:                
        logger.info(f"Node {id}: connecting to {(HOST, UW_APP_TCP_PORT_BASE+id)}")
        #s.connect((HOST, UW_APP_TCP_PORT_BASE+id))
        if not connect_socket(s, (HOST, UW_APP_TCP_PORT_BASE+id)):
            return
        logger.debug(f"Node {id} connected to {(HOST, UW_APP_TCP_PORT_BASE+id)}")
        s.setblocking(0)
        msg = bytes(f"Message from node {id}", encoding="utf-8")
        send_timer = RepeatTimer(interval=send_interval, function=_send_data_nonblocking, args=(s,id,msg))
        if send_interval > 0.0:
            send_timer.start()
        while True:
            readable, _, _ = select.select([s], [], [], 5.0)
            success, data = _recv_data(readable)
            if not success:
                logger.warning(f"Node {id}: disconnected from {(HOST, UW_APP_TCP_PORT_BASE+id)}")
                break
            if data:
                n = datetime.now()
                logger.info(f'{n.strftime("%H:%M:%S")} Node {id} received message (delay: unknown): {data.decode()}')
        send_timer.cancel()  



pos_data = {
    "geodetic": False,
    "x": 0.0,
    "y": 0.0,
    "z": 1000.0,
}


STOP_POSITION_WORKER = False


def pos_worker(id, host, port):
    """Worker thread sending the position data"""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        logger.info(f"Sending position data for node  {id} to {(host, port)}")
        s.setblocking(False)
        while not STOP_POSITION_WORKER:            
            try:
                logger.debug(f"Sending position for node  {id} ...")
                packed_data = struct.pack("<?ddd", pos_data["geodetic"], pos_data["x"], pos_data["y"], pos_data["z"])
                s.sendto(packed_data, (host, port))
            except Exception as e:
                logger.error(f"Error sending position data for node {id}: {e}")
            time.sleep(5.0)
        logger.info(f"Sending position data for node  {id} stopped")


def output_reader(proc, filename_out, filename_err):
    remove_ansi_escape_sequences_in_file = True
    file_out = open(filename_out, 'w')
    file_err = open(filename_err, 'w')
    os.set_blocking(proc.stdout.fileno(), False)
    os.set_blocking(proc.stderr.fileno(), False)
    # print(dir(proc.stdout))
    while (proc.returncode == None):
        # if remove_ansi_escape_sequences_in_file:
        # byte = proc.stdout.read(1)
        line = proc.stdout.readline()
        if line:
            sys.stdout.buffer.write(line)
            sys.stdout.flush()
            if remove_ansi_escape_sequences_in_file:
                file_out.buffer.write(ansi_escape_8bit.sub(b'', line))
            else:
                file_out.buffer.write(line)
            
            continue
        #else:
        #    break
        line = proc.stderr.readline()
        if line:
            logger.error(line.decode())
            file_err.buffer.write(line)
            continue
        time.sleep(0.2)
    logger.info("output_reader(): observed process terminated, closing files")
    file_out.close()
    file_err.close()


# In your destination folder chosen during installation process:
# source environment (if you chose "development" installation mode)
# ./make_environment.sh && source environment (if you chose "release" installation mode)
start_script_template = """#!/bin/bash

set -eo pipefail

BUILD_DIR=%$%{BUILD_DIR}
START_SCRIPT=%$%{START_SCRIPT}

[ -f $BUILD_DIR/environment ] || ([ -f $BUILD_DIR/make_environment.sh ] && pushd $BUILD_DIR && ./make_environment.sh && popd)
. $BUILD_DIR/environment

ns $START_SCRIPT
"""


def main():    
    argparser = ArgumentParser(description='Run network example with node position updates, see uwAppPos_UDP.tmpl for configuration.')
    argparser.add_argument('-n', '--num-nodes', type=int, default=NUM_SEND_NODES, help='Number of sending nodes')
    argparser.add_argument('-t', '--run-time', type=int, default=15, help='Run simulation for given number of seconds')
    argparser.add_argument('-v', '--verbose', action='count', default=0, help="Increase Logger output level, up to three times")
    argparser.add_argument('-b', '--build-dir', required=True, help='DESERT build directory')

    args = argparser.parse_args()
    
    logger.setLevel((logging.ERROR, logging.WARN, logging.INFO, logging.DEBUG)[min(args.verbose, 3)])

    # create ns2 tcl script
    with open('uwAppPos_UDP.tmpl', 'rt') as f:
        s = CustomTemplate(f.read())
        out = s.substitute(TMPL_NO_SENDERS=args.num_nodes, 
                           TMPL_STOPTIME=args.run_time, 
                           TMPL_APP_PORT_BASE=UW_APP_TCP_PORT_BASE, 
                           TMPL_APP_POS_PORT_BASE=UW_APP_UDP_POS_PORT_BASE)
        with open('uwAppPos_UDP.tcl', 'wt') as f1:
            f1.write(out)
               
    # create n2 start script
    s = CustomTemplate(start_script_template)
    out = s.substitute(BUILD_DIR=args.build_dir, START_SCRIPT='uwAppPos_UDP.tcl')
    with open('run.sh', 'wt') as f:
        f.write(out)
    # start process
    ns_proc = subprocess.Popen(['/bin/bash', './run.sh'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    t1 = threading.Thread(target=output_reader, args=(ns_proc, 'ns_run.log', 'ns_run.err'))
    t1.start()
    
    ns_start_time = time.time()  # record start time to observe runtime

    threads = []
    # threads.append(threading.Thread(target=recv_worker, args=(1,)))
    threads.append(threading.Thread(target=recv_send_worker, args=(1,0.0)))
    for i in range(args.num_nodes):
        print(f"Creating send node {i+2}")
        threads.append(threading.Thread(target=recv_send_worker, args=(i+2,5.0)))
    threads.append(threading.Thread(target=pos_worker, args=(1, HOST, UW_APP_UDP_POS_PORT_BASE + 1)))
    for t in threads:
        t.start()
    try:
        while len(threads) > 0:
            time.sleep(1.0)
            # check if double of run time is elapsed and stop ns2 by SIGTERM
            if (time.time() - ns_start_time) > 2*args.run_time:
                logger.info("Double of run time is over, stopping ns2")
                ns_proc_objects = get_process_id_by_name("ns")
                for p in ns_proc_objects:
                    os.kill(p.pid, signal.SIGTERM)
                ns_start_time = time.time()  # reset start time to not kill the processes again in next loop
            
            # stop position worker threads
            if len(get_process_id_by_name("ns")) == 0:
                global STOP_POSITION_WORKER
                STOP_POSITION_WORKER = True
            
            for thread in threads:
                if not thread.is_alive():
                    thread.join()
                    threads.remove(thread)            
        logger.info('All thread connections closed.')
        ns_proc.terminate()  # terminate bash in which ns was running - this stops the output_reader() thread 
        t1.join()
    except KeyboardInterrupt:
        pass    


if __name__ == '__main__':
    main()
