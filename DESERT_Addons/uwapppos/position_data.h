/**
 * @file   position_data.h
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides the definition of PositionData struct
 *
 */

#ifndef POSITION_DATA_H
#define POSITION_DATA_H

// C++ serialization code
#include "archive.h"
// Serialization in Python:
// import struct
// data = struct.pack("<?ddd", geodetic, x, y, z)

struct PositionData
{
    /** If true, x is latitude in [deg] (-90.0,90.0) and y is longitude in [deg] (-180.0,180.0) */
    bool geodetic;
    /** North? East? */
    double x;
    /** East? North? */
    double y;
    /** Above / below sea surface?  [m] */
    double z;

    template <class T>
    void Serialize(T &archive)
    {
        archive & geodetic & x & y & z;
    }
};

#endif /* POSITION_DATA_H */
