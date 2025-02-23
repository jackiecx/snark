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
// 3. Neither the name of the University of Sydney nor the
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


#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <comma/application/command_line_options.h>
#include <comma/application/signal_flag.h>
#include <comma/base/exception.h>
#include <comma/csv/stream.h>
#include <comma/io/stream.h>
#include <comma/io/publisher.h>
#include <comma/string/string.h>
#include <comma/visiting/traits.h>
#include "../message.h"
#include "../sensors.h"
#include "../traits.h"

const char* name() { return "hokuyo-to-csv: "; }

namespace hok = snark::hokuyo;

namespace ip = boost::asio::ip;
/// On exit just send a QT command, although it does not seem to be needed.
class app_exit
{
    ip::tcp::iostream& oss_;
public:
    app_exit( ip::tcp::iostream& oss ) : oss_( oss ) {}
    ~app_exit()
    {
        const hok::state_command stop( "QT" );
        oss_.write( stop.data(), hok::state_command::size );
        oss_.flush();
        oss_.close();
    }
};

static void usage()
{
    std::cerr << std::endl;
    std::cerr << "It puts the laser scanner into scanning mode and broad cast laser data." << std::endl;
    std::cerr << "By default it scans using 1081 steps/rays/data points as fast as possible, you can limit it to 271 steps with --start-step." << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage" << std::endl;
    std::cerr << "    hokuyo-to-csv --laser <host:port> [ --fields t,x,y,z,range,bearing,elevation,intensity ]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "options" << std::endl;
    std::cerr << "*   --laser=:             the TCP connection to the laser <host:port>" << std::endl;
    std::cerr << "    --help,-h:            show this message" << std::endl;
    std::cerr << "    --binary,-b:          output binary equivalent of csv" << std::endl;
    std::cerr << "    --fields=<fields>:    output only given fields" << std::endl;
//     std::cerr << "        default: " << comma::join( comma::csv::names< csv_point >( false ), ',' ) << " (" << comma::csv::format::value< csv_point >() << ")" << std::endl;
    std::cerr << "        t:                timestamp" << std::endl;
    std::cerr << "        x,y,z:            cartesian coordinates in sensor frame, where <0,0,0> is no data" << std::endl;
    std::cerr << "                              range,bearing, elevation or r,b,e: polar coordinates in sensor frame" << std::endl;
    std::cerr << "        i:                intensity of the data point." << std::endl;
    std::cerr << "    --format:             output binary format for given fields to stdout and exit" << std::endl;
    std::cerr << "    --start-step=<0-890>: Scan starting at a start step and go to (step+270) wich covers 67.75\" which is 270\"/4." << std::endl;
    std::cerr << "                          Does not perform a full 270\" scan." << std::endl;
    std::cerr << "    --reboot-on-error:    if failed to put scanner into scanning mode, reboot the scanner." << std::endl;
    std::cerr << "    --omit-error:         if a ray cannot detect an object in range, or very low reflectivity, omit ray from output." << std::endl;
    std::cerr << "    --num-of-scans:       How many scans is requested for ME requests, default is 100 - 0 for continuous ( data verification problem with 0 )." << std::endl;
    std::cerr << "    --scan-break:         How many usec of sleep time between ME request and reponses received before issuing another ME request, default is 20us." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Output format:" << std::endl;
    comma::csv::binary< hok::data_point > binary( "", "" );
    std::cerr << "   format: " << binary.format().string() << " total size is " << binary.format().size() << " bytes" << std::endl;
    std::vector< std::string > names = comma::csv::names< hok::data_point >();
    std::cerr << "   fields: " << comma::join( names, ','  ) << " number of fields: " << names.size() << std::endl;
    std::cerr << std::endl;
    std::cerr << "author:" << std::endl;
    std::cerr << "    dewey nguyen, duynii@gmail.com" << std::endl;
    std::cerr << std::endl;
    exit( -1 );
}

static bool is_omit_error = false;

template < int STEPS >
bool scanning( int start_step, comma::uint32 num_of_scans, // 0 for unlimited
               comma::signal_flag& signaled,
               std::iostream& iostream, comma::csv::output_stream< hok::data_point >& output )
{
    hok::request_md me( true );
    me.header.start_step = start_step;
    me.header.end_step = start_step + STEPS-1;
    me.num_of_scans = num_of_scans;
    
    iostream.write( me.data(), hok::request_md::size );
    iostream.flush();
    
    hok::reply_md state;
    iostream.read( state.data(), hok::reply_md::size );
    
    if( state.request.message_id != me.message_id ) { 
        COMMA_THROW( comma::exception, "message id mismatch for ME status reply, got: " << me.message_id.str() 
                                        << " expected: " << state.request.message_id.str() ); 
    }
    if( state.status.status() != 0 ) 
    { 
        std::ostringstream ss;
        ss << "status reply to ME request is not success: " << state.status.status(); // to change to string
        COMMA_THROW( comma::exception, ss.str() ); 
    }
    
    hok::reply_me_data< STEPS > response; // reply with data
    typename hok::di_data< STEPS >::rays rays;
    while( !signaled && std::cin.good() )
    {
        // TODO just read the status response first, or timeout on read()
        // iostream.read( response.data(), hok::reply_me_data< STEPS >::size );
        int status = hok::read( response, iostream );
        if( status != hok::status::data_success ) 
        {
            COMMA_THROW( comma::exception, "failure dectected when reading data, status: " << status );
        }
        if( response.header.request.message_id != me.message_id ) { 
            COMMA_THROW( comma::exception, "message id mismatch for ME data reply, got: " << me.message_id.str() << " expected: " << response.header.request.message_id.str() ); 
        }
        
        response.encoded.get_values( rays );
        hok::data_point point3d;
        for( std::size_t i=0; i<STEPS; ++i )
        {
            double distance = rays.steps[i].distance();
            if( is_omit_error && 
                ( distance == hok::ust_10lx::distance_nan || distance <= hok::ust_10lx::distance_min ) ) { continue; }
                
                
            point3d.set( distance, 
                         rays.steps[i].intensity(), 
                         hok::ust_10lx::step_to_bearing( i + start_step ) );
            output.write( point3d );


        }

        output.flush();

        // This means we are done
        if( num_of_scans != 0 && response.header.request.num_of_scans == 0 ) { 

            return true; 
        }   
    }
    
    return false;
}

/// Connect to the TCP server within the allowed timeout
/// Needed because comma::io::iostream is not available
bool tcp_connect( const std::string& conn_str, 
                  ip::tcp::iostream& io, 
                  const boost::posix_time::time_duration& timeout=boost::posix_time::seconds(1)
)
{
    std::vector< std::string > v = comma::split( conn_str, ':' );
    boost::asio::io_service service;
    ip::tcp::resolver resolver( service );
    ip::tcp::resolver::query query( v[0] == "localhost" ? "127.0.0.1" : v[0], v[1] );
    ip::tcp::resolver::iterator it = resolver.resolve( query );
    
    io.expires_from_now( timeout );
    io.connect( it->endpoint() );
    
    io.expires_at( boost::posix_time::pos_infin );
    
    return io.error() == 0;
} 

int main( int ac, char** av )
{
    comma::signal_flag signaled;
    comma::command_line_options options( ac, av );
    if( options.exists( "--help,-h" ) ) { usage(); }
    
    try
    {
        is_omit_error = options.exists( "--omit-error" );

        comma::uint32 scan_break = options.value< comma::uint32 > ( "--scan-break", 20 ); // time in us
        comma::uint32 num_of_scans = options.value< comma::uint32 > ( "--num-of-scans", 100 ); // time in us
        
        // Sets up output data
        comma::csv::options csv;
        csv.fields = options.value< std::string >( "--fields", "" );
        std::vector< std::string > v = comma::split( csv.fields, ',' );
        for( std::size_t i = 0; i < v.size(); ++i ) // convenience shortcuts
        {
            if( v[i] == "i" ) { v[i] = "intensity"; }
            else if( v[i] == "r" ) { v[i] = "range"; }
            else if( v[i] == "b" ) { v[i] = "bearing"; }
            else if( v[i] == "e" ) { v[i] = "elevation"; }
            else if( v[i] == "t" ) { v[i] = "timestamp"; }
        }
        csv.fields = comma::join( v, ',' );
        csv.full_xpath = false;
        // see sick-ldmrs-to-csv
        if( options.exists( "--format" ) ) { std::cout << comma::csv::format::value< hok::data_point >( csv.fields, false ) << std::endl; return 0; }
        if( options.exists( "--binary,-b" ) ) csv.format( comma::csv::format::value< hok::data_point >( csv.fields, false ) );
        comma::csv::output_stream< hok::data_point > output( std::cout, csv );  
        
        if( options.exists( "--output-samples" ) )
        {
            hok::data_point pt;
            pt.x = 1; pt.y = 2; pt.z = 3;
            pt.intensity = 100;
            while( !signaled && std::cout.good() )
            {
                pt.timestamp = boost::posix_time::microsec_clock::local_time();
                output.write( pt );
                usleep( 0.1 * 1000000u );
            }
            
            return 0;
        }
        
        /// Connect to the laser
        ip::tcp::iostream iostream;
        if( !tcp_connect( options.value< std::string >( "--laser" ), iostream ) ) {
            COMMA_THROW( comma::exception, "failed to connect to the hokuyo laser at: " << options.value< std::string >( "--laser" ) );
        }
        
        bool reboot_on_error = options.exists( "--reboot-on-error" );
        
        // Let put the laser into scanning mode
        {
            hok::state_command start( "BM" ); // starts transmission
            hok::state_reply start_reply;
    
            iostream.write( start.data(), hok::state_command::size  );
            iostream.flush();
            
            comma::io::select select;
            select.read().add( iostream.rdbuf()->native() );
            
            select.wait( 1 ); // wait one select for reply, it can be much smaller
            if( !select.read().ready( iostream.rdbuf()->native() ) ) { 
                COMMA_THROW( comma::exception, "no reply received from laser scanner after a startup (BM) command: " << std::string( start.data(), hok::state_command::size ) ); 
            }
            iostream.read( start_reply.data(), hok::state_reply::size  );
            
            if( start_reply.status() != 0 && 
                start_reply.status() != 10 &&
                start_reply.status() != 2 ) // 0 = success, 2 seems to be returned when it is already in scanning mode but idle
            {
                if( reboot_on_error )
                {
                    // it must be sent twice within one second
                    iostream << "RB\n"; iostream.flush();
                    iostream << "RB\n"; iostream.flush();
                    sleep( 1 );
                }
                COMMA_THROW( comma::exception, std::string("Starting laser with BM command failed, status: ") + std::string( start_reply.status.data(), 2 ) ); 
            }
        }
    
        {
            app_exit onexit( iostream );
        
            // it is higher than 1080 because 0 is a step
            static const int MAX_STEPS = 1081;
            static const int SCANS = num_of_scans;
            
            comma::uint32 start_encoder_step = 0;
            if( options.exists( "--start-step" ) ) 
            { 
                static const int SMALL_STEPS = 271;
                start_encoder_step = options.value< comma::uint32 >( "--start-step", 0 );
                if( start_encoder_step >= ( hok::ust_10lx::step_max - SMALL_STEPS ) ) { COMMA_THROW( comma::exception, "start step is too high" ); }

                while( scanning< SMALL_STEPS >( start_encoder_step, SCANS, signaled, iostream, output ) ) { usleep( scan_break ); }
            }
            else {
                while( scanning< MAX_STEPS >(   start_encoder_step, SCANS, signaled, iostream, output ) ) { usleep( scan_break ); }
            }
        }
    }
    catch( std::exception& ex )
    {
        std::cerr << name() << ex.what() << std::endl; return 1;
    }
    catch( ... )
    {
        std::cerr << name() << "unknown exception" << std::endl; return 1;
    }
    
}