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
    logging.basicConfig(format='PYTHON %(asctime)s.%(msecs)03d %(levelname)s: %(message)s', level=logging.DEBUG,
                        datefmt='%H:%M:%S')

file_handler = logging.FileHandler("python.log",mode='w+')
formatter = logging.Formatter("%(asctime)s - %(threadName)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)
logger.addHandler(file_handler)

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
UW_APP_PORT_BASE = 4000  # receive port of uwAppPos
UW_APP_SEND_PORT_BASE = 5000
NUM_SEND_NODES = 2
UW_APP_UDP_POS_PORT_BASE = 6000


class RepeatTimer(threading.Timer):

    def run(self):
        while not self.finished.wait(self.interval):
            self.function(*self.args, **self.kwargs)


class SingleNode(threading.Thread):
    def __init__(self, node_id: int, send_interval: float) -> None:
        """
        id: node id
        send_interval: send interval in [s], set to 0.0 to not send messages
        """
        super().__init__(name=f"NodeThread{node_id}")
        self.node_id = node_id
        self.send_interval = send_interval
        self.should_stop = False


    def stop(self):
        """stop the thread."""
        self.should_stop = True

    def send_payload(self, s: socket, msg, address):
        logger.info(f"Sending '{msg}' to {address}")
        s.sendto(msg, address)

    def run(self):
        """ Worker thread, receives (and optionally sends) messages to ns2.
            Sending is only active when send_interval > 0.0
        """
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            logger.info(f"Node {self.node_id}: UDP socket binding to {(HOST, UW_APP_SEND_PORT_BASE + self.node_id)}")
            s.bind((HOST, UW_APP_SEND_PORT_BASE + self.node_id))
            s.settimeout(0.2)
            msg = bytes(f"Message from node {self.node_id}", encoding="utf-8")
            send_timer = RepeatTimer(interval=self.send_interval,
                                     function=self.send_payload,
                                     args=(s, msg, (HOST, UW_APP_PORT_BASE + self.node_id)))
            if self.send_interval > 0.0:
                send_timer.start()
            while not self.should_stop:
                try:
                    data, addr = s.recvfrom(1024)  # buffer size is 1024 bytes
                    if data:
                        n = datetime.now()
                        logger.info(
                            f'{n.strftime("%H:%M:%S")} Node {self.node_id} received message (delay: unknown): {data.decode()}')
                except TimeoutError:
                    pass
            send_timer.cancel()


class PosWorker(threading.Thread):
    def __init__(self, node_id: int,
                 send_interval: float,
                 send_address: tuple,
                 pos_data: dict,
                 speed: dict = None) -> None:
        """
        id: node id
        send_interval: send interval in [s], set to 0.0 to not send messages
        """
        super().__init__(name=f"PosWorker{node_id}")
        if speed is None:
            speed = {"x": 1.0, "y": 0.0, "z": 1.5}
        self.node_id = node_id
        self.send_interval = send_interval
        self.should_stop = False
        self.send_address = send_address
        self.pos_data = pos_data
        self.speed = speed

    def stop(self):
        """stop the thread."""
        self.should_stop = True

    def run(self):
        """Worker thread sending the position data"""
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            logger.info(f"Sending position data for node  {self.node_id} to {self.send_address}")
            # s.setblocking(False)
            last_pos_update = 0.0
            while not self.should_stop:
                try:
                    now_dt = datetime.utcnow()
                    now = time.mktime(now_dt.timetuple()) + now_dt.microsecond / 1e6
                    if last_pos_update > 0.0:
                        dt = now - last_pos_update
                        if dt > 0.0:
                            self.pos_data["x"] += dt * self.speed["x"]
                            self.pos_data["y"] += dt * self.speed["y"]
                            self.pos_data["z"] += dt * self.speed["z"]
                    last_pos_update = now
                    logger.info(f"Sending position for node {self.node_id}: {self.pos_data}")
                    packed_data = struct.pack("<?ddd", self.pos_data["geodetic"],
                                              self.pos_data["x"], self.pos_data["y"], self.pos_data["z"])
                    s.sendto(packed_data, self.send_address)
                except Exception as e:
                    logger.error(f"Error sending position data for node {self.node_id}: {e}", exc_info=True)
                time.sleep(5.0)
            logger.info(f"Sending position data for node  {self.node_id} stopped")


def output_reader(proc, filename_out, filename_err):
    remove_ansi_escape_sequences_in_file = True
    file_out = open(filename_out, 'w')
    file_err = open(filename_err, 'w')
    os.set_blocking(proc.stdout.fileno(), False)
    os.set_blocking(proc.stderr.fileno(), False)
    while proc.returncode is None:
        line = proc.stdout.readline()
        if line:
            sys.stdout.buffer.write(line)
            sys.stdout.flush()
            if remove_ansi_escape_sequences_in_file:
                file_out.buffer.write(ansi_escape_8bit.sub(b'', line))
            else:
                file_out.buffer.write(line)
            continue
        # else:
        #    break
        line = proc.stderr.readline()
        if line:
            logger.error(line.decode())
            file_err.buffer.write(line)
            continue
        # sleep only if no data was read from both
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
    argparser = ArgumentParser(
        description='Run network example with node position updates, see uwAppPos_UDP.tmpl for configuration.')
    argparser.add_argument('-n', '--num-nodes', type=int, default=NUM_SEND_NODES, help='Number of sending nodes')
    argparser.add_argument('-s', '--no-ns-start', action='store_true', help='Do not start ns')  # for remote execution
    argparser.add_argument('-t', '--run-time', type=int, default=15, help='Run simulation for given number of seconds')
    argparser.add_argument('-v', '--verbose', action='count', default=0,
                           help="Increase Logger output level, up to three times")
    argparser.add_argument('-b', '--build-dir', required=True, help='DESERT build directory')

    args = argparser.parse_args()
    start_ns = not args.no_ns_start
    logger.setLevel((logging.ERROR, logging.WARN, logging.INFO, logging.DEBUG)[min(args.verbose, 3)])

    script = 'uwAppPos_UDP'
    # create ns2 tcl script
    with open(f'{script}.tmpl', 'rt') as f:
        s = CustomTemplate(f.read())
        out = s.substitute(TMPL_NO_SENDERS=args.num_nodes,
                           TMPL_PROTOCOL="udp",
                           TMPL_STOPTIME=args.run_time,
                           TMPL_APP_SEND_HOST=HOST,
                           TMPL_APP_PORT_BASE=UW_APP_PORT_BASE,
                           TMPL_APP_SEND_PORT_BASE=UW_APP_SEND_PORT_BASE,
                           TMPL_APP_POS_PORT_BASE=UW_APP_UDP_POS_PORT_BASE)
        with open(f'{script}.tcl', 'wt') as f1:
            f1.write(out)

    # create n2 start script
    s = CustomTemplate(start_script_template)
    out = s.substitute(BUILD_DIR=args.build_dir, START_SCRIPT=f'{script}.tcl')
    with open('run.sh', 'wt') as f:
        f.write(out)
    if start_ns:
        # start process
        ns_proc = subprocess.Popen(['/bin/bash', './run.sh'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        t1 = threading.Thread(target=output_reader, args=(ns_proc, 'ns_run.log', 'ns_run.err'))
        t1.start()
        ns_start_time = time.time()  # record start time to observe runtime

    threads = []
    threads.append(SingleNode(1, 0.0))
    for i in range(args.num_nodes):
        print(f"Creating send node {i + 2}")
        threads.append(SingleNode(i + 2, 5.0))
    threads.append(PosWorker(1,
                             5.0,
                             (HOST, UW_APP_UDP_POS_PORT_BASE + 1),
                             {
                                 "geodetic": False,
                                 "x": 0.0,
                                 "y": 0.0,
                                 "z": 100.0,
                             }
                             ))
    for t in threads:
        t.start()
    try:
        while len(threads) > 0:
            time.sleep(1.0)
            if start_ns:
                # check if double of run time is elapsed and stop ns2 by SIGTERM
                if (time.time() - ns_start_time) > 2 * args.run_time:
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
                        thread.stop()

                for thread in threads:
                    if not thread.is_alive():
                        thread.join()
                        threads.remove(thread)
        logger.info('All thread connections closed.')
        if start_ns:
            ns_proc.terminate()  # terminate bash in which ns was running - this stops the output_reader() thread 
            t1.join()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
