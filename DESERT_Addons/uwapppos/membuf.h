#pragma once

/*
  from memory:
  int bufSize = 128;
  char buffer[bufSize];
  memory_buffer sbuf(buffer, buffer+bufSize);
  std::istream in(&sbuf);
  in.seekg(0, std::ios_base::beg);  // reset read position
*/

#include <streambuf>
#include <iostream>

class memory_buffer : public std::streambuf
{
public:
    /** Initialize with begin and end of buffer */
    memory_buffer(char *begin, char *end) : begin(begin), end(end)
    {
        reset();
    }
    memory_buffer(char *s, std::size_t n) : begin(s), end(s + n)
    {
        reset();
    }
    /** Get the size of the underlying buffer, non-standard */
    std::streamsize size() const
    {
        return std::streamsize(pptr() - pbase());
    }

protected:
    /** Resets read and write position to the beginning of the buffer */
    void reset()
    {
        setg(begin, begin, end);
        setp(begin, end); // put pointer is automatically set to the beginning
    }

    virtual pos_type seekoff(
        off_type off, std::ios_base::seekdir dir,
        std::ios_base::openmode mode) override
    {
        auto pos = gptr();
        if (dir == std::ios_base::cur)
            pos += off;
        else if (dir == std::ios_base::end)
            pos = egptr() + off;
        else if (dir == std::ios_base::beg)
            pos = eback() + off;

        // check bunds
        if (pos < eback())
            return std::streambuf::pos_type(-1);
        else if (pos > egptr())
            return std::streambuf::pos_type(-1);

        if (mode & std::ios_base::in)
        { // reset read pointers
            setg(eback(), pos, egptr());
        }
        if (mode & std::ios_base::out)
        { // reset write pointers
            setp(pbase(), epptr());
            pbump(pos - eback());
        }
        return gptr() - eback();
    }
    virtual pos_type seekpos(std::streampos pos, std::ios_base::openmode mode) override
    {
        return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
    }

private:
    char *begin, *end;
};
