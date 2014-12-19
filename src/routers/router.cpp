// $Id$

/*
 Copyright (c) 2007-2012, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this 
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*router.cpp
 *
 *The base class of either iq router or event router
 *contains a list of channels and other router configuration variables
 *
 *The older version of the simulator uses an array of flits and credit to 
 *simulate the channels. Newer version ueses flitchannel and credit channel
 *which can better model channel delay
 *
 *The older version of the simulator also uses vc_router and chaos router
 *which are replaced by iq rotuer and event router in the present form
 */

#include "booksim.hpp"
#include <iostream>
#include <cassert>
#include "router.hpp"

//////////////////Sub router types//////////////////////
#include "iq_router.hpp"
#include "event_router.hpp"
#include "chaos_router.hpp"
///////////////////////////////////////////////////////

int const Router::STALL_BUFFER_BUSY = -2;
int const Router::STALL_BUFFER_CONFLICT = -3;
int const Router::STALL_BUFFER_FULL = -4;
int const Router::STALL_BUFFER_RESERVED = -5;
int const Router::STALL_CROSSBAR_CONFLICT = -6;
int const Router::NODE_TYPE_CACHE_NODE = 0;
int const Router::NODE_TYPE_PROC_NODE = 1;
int const Router::NODE_TYPE_EMPTY_NODE = 2;
//int const Router::CACHE_LINE_SIZE = 1 * 1024 * 1024 / 4; // 1MB, word addressable <-- what was this again??
int const Router::NUM_CACHE_LINES_PER_BANK = 256; // each bank is 16KB --> 16KB / 64B = 256
int const Router::CACHE_LINE_SIZE = 64; // each line is 64 bytes

Router::Router( const Configuration& config,
		Module *parent, const string & name, int id,
		int inputs, int outputs, int node_type = 1) :
TimedModule( parent, name ), _id( id ), _inputs( inputs ), _outputs( outputs ),
   _partial_internal_cycles(0.0)
{
  this->node_type = node_type;

  _crossbar_delay   = ( config.GetInt( "st_prepare_delay" ) + 
			config.GetInt( "st_final_delay" ) );
  _credit_delay     = config.GetInt( "credit_delay" );
  _input_speedup    = config.GetInt( "input_speedup" );
  _output_speedup   = config.GetInt( "output_speedup" );
  _internal_speedup = config.GetFloat( "internal_speedup" );
  _classes          = config.GetInt( "classes" );

#ifdef TRACK_FLOWS
  _received_flits.resize(_classes, vector<int>(_inputs, 0));
  _stored_flits.resize(_classes);
  _sent_flits.resize(_classes, vector<int>(_outputs, 0));
  _active_packets.resize(_classes);
  _outstanding_credits.resize(_classes, vector<int>(_outputs, 0));
#endif

#ifdef TRACK_STALLS
  _buffer_busy_stalls.resize(_classes, 0);
  _buffer_conflict_stalls.resize(_classes, 0);
  _buffer_full_stalls.resize(_classes, 0);
  _buffer_reserved_stalls.resize(_classes, 0);
  _crossbar_conflict_stalls.resize(_classes, 0);
#endif

}

void Router::AddInputChannel( FlitChannel *channel, CreditChannel *backchannel )
{
  _input_channels.push_back( channel );
  _input_credits.push_back( backchannel );
  channel->SetSink( this, _input_channels.size() - 1 ) ;
}

void Router::AddOutputChannel( FlitChannel *channel, CreditChannel *backchannel )
{
  _output_channels.push_back( channel );
  _output_credits.push_back( backchannel );
  _channel_faults.push_back( false );
  channel->SetSource( this, _output_channels.size() - 1 ) ;
}

void Router::Evaluate( )
{
  _partial_internal_cycles += _internal_speedup;
  while( _partial_internal_cycles >= 1.0 ) {
    _InternalStep( );
    _partial_internal_cycles -= 1.0;
  }
}

void Router::OutChannelFault( int c, bool fault )
{
  assert( ( c >= 0 ) && ( (size_t)c < _channel_faults.size( ) ) );

  _channel_faults[c] = fault;
}

bool Router::IsFaultyOutput( int c ) const
{
  assert( ( c >= 0 ) && ( (size_t)c < _channel_faults.size( ) ) );

  return _channel_faults[c];
}

/*Router constructor*/
Router *Router::NewRouter( const Configuration& config,
			   Module *parent, const string & name, int id,
			   int inputs, int outputs, int node_type)
{
  const string type = config.GetStr( "router" );
  Router *r = NULL;
  if ( type == "iq" ) {
    r = new IQRouter( config, parent, name, id, inputs, outputs, node_type);
  } else if ( type == "event" ) {
    r = new EventRouter( config, parent, name, id, inputs, outputs , node_type);
  } else if ( type == "chaos" ) {
    r = new ChaosRouter( config, parent, name, id, inputs, outputs , node_type);
  } else {
    cerr << "Unknown router type: " << type << endl;
  }
  /*For additional router, add another else if statement*/
  /*Original booksim specifies the router using "flow_control"
   *we now simply call these types. 
   */

  return r;
}

Router *Router::NewRouter( const Configuration& config,
			   Module *parent, const string & name, int id,
			   int inputs, int outputs) {
    return NewRouter(config, parent, name, id, inputs, outputs, 1);
}


bool Router::FindCacheLine(long address) {
    assert(NODE_TYPE_CACHE_NODE == this->node_type);
    return cache_lines.find(address) != cache_lines.end();
}

// returns the address of the LRU line
long Router::FindLRU() {
    assert(!cache_lines.empty());
    long addr = cache_lines.begin()->first;
    int time = cache_lines.begin()->second;
    for (std::map<long,int>::iterator it=cache_lines.begin(); it!=cache_lines.end(); ++it)
    {
        if(it->second < time)
        {
            addr = it->first;
            time = it->second;
        }
    }
    return addr;
}

int Router::FindLRUTime() {
    if(cache_lines.empty())
    {
        return -1;
    }
    long addr = cache_lines.begin()->first;
    int time = cache_lines.begin()->second;
    for (std::map<long,int>::iterator it=cache_lines.begin(); it!=cache_lines.end(); ++it)
    {
        if(it->second < time)
        {
            addr = it->first;
            time = it->second;
        }
    }
    return time;
}


long Router::GetAlignedAddress(long addr)
{
    long alignedAddr = addr/CACHE_LINE_SIZE * CACHE_LINE_SIZE;
    return alignedAddr;
}

void Router::AddCacheLine(long address, int time) {
    assert(NODE_TYPE_CACHE_NODE == this->node_type);

    // check if cache bank is full. if so, need to evict a
    // cache line before inserting a new one
    if(cache_lines.size() >= NUM_CACHE_LINES_PER_BANK)
    {
        int key = FindLRU();
        cache_lines.erase(key);
        vertical_tendency.erase(address);
        horizontal_tendency.erase(address);
    }

    // insert new cache line
    cache_lines.insert(std::pair<unsigned long, int>(address, time));
    vertical_tendency.insert(std::pair<unsigned long, unsigned char>(address, 1));
    horizontal_tendency.insert(std::pair<unsigned long, unsigned char>(address, 1));

}

void Router::RemoveCacheLine(long address) {
    assert(NODE_TYPE_CACHE_NODE == this->node_type);
    cache_lines.erase(address);
    vertical_tendency.erase(address);
    horizontal_tendency.erase(address);
}

// check if this cache bank handles the passed in address
bool Router::HandlesAddress(long address) {
    if (this->node_type != NODE_TYPE_CACHE_NODE) {
        return false;
    }

    for (int i = 0; i < address_ranges.size(); i++) {
        // assumes addresses 
        if (address_ranges[i].first <= address
            && address <= address_ranges[i].second) {
            return true;
        }
    }

    return false;
}


void Router::NewReplyTrack (int packetId, int size) {
    replyCounts.insert(make_pair(packetId, size));
}

// this function updates the reply count received so far and returns whether all replies have been received yet
bool Router::ReceivedAllReply (int packetId) {
    int curCount = replyCounts[packetId];
    cout << "replyCounts[" << packetId << "]=" << curCount << endl;
    if(curCount == 1)
    {
        // all replies have been received
        replyCounts.erase(packetId);
        return true;
    }
    else
    {
        replyCounts[packetId] = curCount - 1;
        return false;
    }
}

void Router::NewReplacementTrack (int packetId) {
    replacementRequest.insert(make_pair(packetId, true));
}

void Router::UpdateReplacementTrack (int packetId, bool cacheHit) {
    if(cacheHit)
    {
        cout << "CACHE HIT!!" << endl;
        replacementRequest[packetId] = false;
    }
}

// Note: this removes the replacement status for the packet once it's called
bool Router::GetReplacementTrack (int packetId) {
    bool replace = replacementRequest[packetId];
    replacementRequest.erase(packetId);
    cout << " (replace=" << replace << ") " << endl;
    return replace;
}

int Router::GetHorizontalTendency(unsigned long addr){
    return horizontal_tendency[addr];
}

int Router::GetVerticalTendency(unsigned long addr){
    return vertical_tendency[addr];
}

void Router::UpdateTendency(unsigned long addr, unsigned char dir) {
    // update the tendency
    switch(dir) {
        case 0: // NORTH
            if (vertical_tendency[addr] > 0) {
                vertical_tendency[addr]--;
                cout << addr << " is going NORTH\n";
            }
            break;
        case 1: // SOUTH
            if (vertical_tendency[addr] < 3) {
                vertical_tendency[addr]++;
                cout << addr << " is going SOUTH\n";
            }
            break;
        case 2: // EAST
            if (horizontal_tendency[addr] > 0) {
                horizontal_tendency[addr]--;
                cout << addr << " is going EAST\n";
            }
            break;
        case 3: // WEST
            if (horizontal_tendency[addr] < 3) {
                horizontal_tendency[addr]++;
                cout << addr << " is going WEST\n";
            }
            break;
        default:
            assert(0);
    }
}

void Router::NewRequestTimeTrack (int packetId, int time) {
    requestTime.insert(make_pair(packetId, time));
}

void Router::DeleteRequestTimeTrack (int packetId) {
    requestTime.erase(packetId);
}

int Router::GetRequestTimeTrack (int packetId) {
    return requestTime[packetId];
}
