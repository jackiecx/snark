// This file is part of snark, a generic and flexible library for robotics research
// Copyright (c) 2011 The University of Sydney
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All advertising materials mentioning features or use of this software
//    must display the following acknowledgement:
//    This product includes software developed by the The University of Sydney.
// 4. Neither the name of the The University of Sydney nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
// HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef SNARK_MATH_RBE_H
#define SNARK_MATH_RBE_H

#include <Eigen/Core>
#include <comma/math/compare.h>

namespace snark {

/// polar point definition (range-bearing-elevation)
class range_bearing_elevation
{
public:
    /// constructors
    range_bearing_elevation(){}
    range_bearing_elevation( double r, double b, double e ): m_rbe( r, b, e ) {}

    /// return coordinates
    double range() const { return m_rbe[0]; }
    double bearing() const { return m_rbe[1]; }
    double elevation() const { return m_rbe[2]; }

    /// set coordinates
    void range( double t );
    void bearing( double t );
    void elevation( double t );

    /// for brevity's sake
    double r() const { return range(); }
    double b() const { return bearing(); }
    double e() const { return elevation(); }
    void r( double t ) { range( t ); }
    void b( double t ) { bearing( t ); }
    void e( double t ) { elevation( t ); }

    Eigen::Vector3d to_cartesian() const;
    const range_bearing_elevation& from_cartesian( double x, double y, double z );
    const range_bearing_elevation& from_cartesian( const Eigen::Vector3d& xyz );

private:
    Eigen::Vector3d m_rbe;
};

/// a short-hand for lazy
typedef range_bearing_elevation rbe;

} // namespace snark {

#endif // SNARK_MATH_RBE_H
