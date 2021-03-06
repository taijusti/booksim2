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

/*kn.cpp
 *
 *Meshs, cube, torus
 *
 */

#include <iostream>
#include <fstream>
#include "booksim.hpp"
#include <cassert>
#include <vector>
#include <sstream>
#include "kncube.hpp"
#include "random_utils.hpp"
#include "misc_utils.hpp"
 //#include "iq_router.hpp"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

KNCube::KNCube( const Configuration &config, const string & name, bool mesh ) :
Network( config, name )
{
  _mesh = mesh;

  _ComputeSize( config );
  _Alloc( );
  _BuildNet( config );
  _AssignCacheSet( config );

}

void KNCube::_ComputeSize( const Configuration &config )
{
  _k = config.GetInt( "k" );
  _n = config.GetInt( "n" );

  gK = _k; gN = _n;
  _size     = powi( _k, _n );
  _channels = 2*_n*_size;

  _nodes = _size;
}

void KNCube::PrintNet() {
    int row = 0;

    if (2 != _n) {
        return;
    }

    for (int node = 0; node < _size; node++) {
        cout << _routers[node]->GetNodeType();
        row++;
        if (row == _k) {
            row = 0;
            cout << "\n";
        }
    }
}

void KNCube::RegisterRoutingFunctions() {

}


int KNCube::getSet( long addr )
{
    int numSets = _cacheSets.size();
    int set = addr % numSets;
    //cout << "getSet: " << addr << " = set " << set << endl;
    return set;
}

void KNCube::_AssignCacheSet( const Configuration &config )
{
  if(0 == config.GetStr("set_map").compare(""))
  {
    // TODO: no file specified, every node is it's own set
  }
  else
  {
    string line;
    int node = 0;
    ifstream set_map_file (config.GetStr("set_map").c_str());
    getline(set_map_file, line);
    while(0 != line.compare(""))
    {
      char * cstr = new char [line.length()+1];
      strcpy(cstr, line.c_str());
      char *word;
      word = strtok(cstr, " ");
      while(NULL != word)
      {
        int setNum = atoi(word);
        if(setNum >= 0) // -1 are assigned to processors in the map file
        {
          if(_cacheSets.size() <= setNum)
          {
            _cacheSets.resize(setNum+1); //setNum starts from 0
          }
          (_cacheSets[setNum]).push_back(node);
        }
        node++;
        word = strtok(NULL, " ");
      }
      delete(cstr);
      getline(set_map_file, line);
    }

    globalLRU.resize(_cacheSets.size());
    LRUNode.resize(_cacheSets.size());
    for(int i = 0; i < _cacheSets.size(); i++)
    {
        globalLRU[i] = 0;
        LRUNode[i] = _cacheSets[i][0];
    }
      
  }

  cout << "==============================================" << endl;
  cout << "There are " << _cacheSets.size() << " sets in total" << endl;
  for(int i = 0; i < 8; i++)
  {
    cout << "set " << i << ": ";
    for(int j=0; j < _cacheSets[i].size(); j++)
    {
      cout << "[" << _cacheSets[i][j] << "]";
    }
    cout << endl;
  }
  cout << "==============================================" << endl;


}

void KNCube::_BuildNet( const Configuration &config )
{
  int left_node;
  int right_node;

  int right_input;
  int left_input;

  int right_output;
  int left_output;
  map<int, vector<int> > node_config;
  bool isAddrTrace = false;

  ifstream node_map_file ( config.GetStr("node_map").c_str());

  // parse in the node map and address map
  if (node_map_file != "") {
    string line;
    char c_line [100];
    int node = 0;
    int procNum = 0;
    int type = 0;
    unsigned long begin_addr = 0;
    unsigned long end_addr = 0;
    char * begin_addr_str;
    char * end_addr_str;

    assert(node_map_file.is_open());
    isAddrTrace = true;

#if 1
    getline(node_map_file, line);
    while(0 != line.compare(""))
    {
      char * cstr = new char [line.length()+1];
      strcpy(cstr, line.c_str());
      char *word;
      word = strtok(cstr, " ");
      while(NULL != word)
      {
        type = atoi(word);
        node_config[node].push_back(type);
        if(type == 1)
        {
            procToNodeMap.emplace(procNum, node);
            procNum++;
        }
        node++;
        word = strtok(NULL, " ");
      }
      delete(cstr);
      getline(node_map_file, line);
    }

#endif
#if 0
    while (getline(node_map_file, line)) {
      strcpy(c_line, line.c_str());
      //memcpy(c_line, line.c_str(), 300);
      node = atoi(strtok(c_line, " "));
      type = atoi(strtok(NULL, " "));
      node_config[node].push_back(type);

      begin_addr_str = strtok(NULL, " ");
      end_addr_str = strtok(NULL, " ");
      while (begin_addr_str != NULL) {
          begin_addr = atol(begin_addr_str);
          end_addr = atol(end_addr_str);
          node_config[node].push_back(begin_addr);
          node_config[node].push_back(end_addr);
          begin_addr_str = strtok(NULL, " ");  
          end_addr_str = strtok(NULL, " ");  
      }
    }
#endif
    node_map_file.close();
    assert(_size == node_config.size());
  }

  //latency type, noc or conventional network
  ostringstream router_name;
  bool use_noc_latency;
  use_noc_latency = (config.GetInt("use_noc_latency")==1);
  
  for ( int node = 0; node < _size; ++node ) {

    router_name << "router";
    
    if ( _k > 1 ) {
      for ( int dim_offset = _size / _k; dim_offset >= 1; dim_offset /= _k ) {
    router_name << "_" << ( node / dim_offset ) % _k;
      }
    }

    if (node_config.size() > 0) {
        _routers[node] = Router::NewRouter( config, this, router_name.str( ), 
                    node, 2*_n + 1, 2*_n + 1, node_config[node][0]);
    }
    else {
        _routers[node] = Router::NewRouter( config, this, router_name.str( ), 
                    node, 2*_n + 1, 2*_n + 1);
    }
    _timed_modules.push_back(_routers[node]);

    router_name.str("");

    for ( int dim = 0; dim < _n; ++dim ) {

      //find the neighbor 
      left_node  = _LeftNode( node, dim );
      right_node = _RightNode( node, dim );

      //
      // Current (N)ode
      // (L)eft node
      // (R)ight node
      //
      //   L--->N<---R
      //   L<---N--->R
      //

      // torus channel is longer due to wrap around
      int latency = _mesh ? 1 : 2 ;

      //get the input channel number
      right_input = _LeftChannel( right_node, dim );
      left_input  = _RightChannel( left_node, dim );

      //add the input channel
      _routers[node]->AddInputChannel( _chan[right_input], _chan_cred[right_input] );
      _routers[node]->AddInputChannel( _chan[left_input], _chan_cred[left_input] );

      //set input channel latency
      if(use_noc_latency){
        _chan[right_input]->SetLatency( latency );
        _chan[left_input]->SetLatency( latency );
        _chan_cred[right_input]->SetLatency( latency );
        _chan_cred[left_input]->SetLatency( latency );
      } else {
        _chan[left_input]->SetLatency( 1 );
        _chan_cred[right_input]->SetLatency( 1 );
        _chan_cred[left_input]->SetLatency( 1 );
        _chan[right_input]->SetLatency( 1 );
      }
      //get the output channel number
      right_output = _RightChannel( node, dim );
      left_output  = _LeftChannel( node, dim );
      
      //add the output channel
      _routers[node]->AddOutputChannel( _chan[right_output], _chan_cred[right_output] );
      _routers[node]->AddOutputChannel( _chan[left_output], _chan_cred[left_output] );

      //set output channel latency
      if(use_noc_latency){
        _chan[right_output]->SetLatency( latency );
        _chan[left_output]->SetLatency( latency );
        _chan_cred[right_output]->SetLatency( latency );
        _chan_cred[left_output]->SetLatency( latency );
      } else {
        _chan[right_output]->SetLatency( 1 );
        _chan[left_output]->SetLatency( 1 );
        _chan_cred[right_output]->SetLatency( 1 );
        _chan_cred[left_output]->SetLatency( 1 );

      }
    }
    //injection and ejection channel, always 1 latency
    _routers[node]->AddInputChannel( _inject[node], _inject_cred[node] );
    _routers[node]->AddOutputChannel( _eject[node], _eject_cred[node] );
    _inject[node]->SetLatency( 1 );
    _eject[node]->SetLatency( 1 );
  }

  PrintNet();
}

int KNCube::_LeftChannel( int node, int dim )
{
  // The base channel for a node is 2*_n*node
  int base = 2*_n*node;
  // The offset for a left channel is 2*dim + 1
  int off  = 2*dim + 1;

  return ( base + off );
}

int KNCube::_RightChannel( int node, int dim )
{
  // The base channel for a node is 2*_n*node
  int base = 2*_n*node;
  // The offset for a right channel is 2*dim 
  int off  = 2*dim;
  return ( base + off );
}

int KNCube::_LeftNode( int node, int dim )
{
  int k_to_dim = powi( _k, dim );
  int loc_in_dim = ( node / k_to_dim ) % _k;
  int left_node;
  // if at the left edge of the dimension, wraparound
  if ( loc_in_dim == 0 ) {
    left_node = node + (_k-1)*k_to_dim;
  } else {
    left_node = node - k_to_dim;
  }

  return left_node;
}

int KNCube::_RightNode( int node, int dim )
{
  int k_to_dim = powi( _k, dim );
  int loc_in_dim = ( node / k_to_dim ) % _k;
  int right_node;
  // if at the right edge of the dimension, wraparound
  if ( loc_in_dim == ( _k-1 ) ) {
    right_node = node - (_k-1)*k_to_dim;
  } else {
    right_node = node + k_to_dim;
  }

  return right_node;
}

int KNCube::GetN( ) const
{
  return _n;
}

int KNCube::GetK( ) const
{
  return _k;
}

/*legacy, not sure how this fits into the own scheme of things*/
void KNCube::InsertRandomFaults( const Configuration &config )
{
  int num_fails = config.GetInt( "link_failures" );
  
  if ( _size && num_fails ) {
    vector<long> save_x;
    vector<double> save_u;
    SaveRandomState( save_x, save_u );
    int fail_seed;
    if ( config.GetStr( "fail_seed" ) == "time" ) {
      fail_seed = int( time( NULL ) );
      cout << "SEED: fail_seed=" << fail_seed << endl;
    } else {
      fail_seed = config.GetInt( "fail_seed" );
    }
    RandomSeed( fail_seed );

    vector<bool> fail_nodes(_size);

    for ( int i = 0; i < _size; ++i ) {
      int node = i;

      // edge test
      bool edge = false;
      for ( int n = 0; n < _n; ++n ) {
    if ( ( ( node % _k ) == 0 ) ||
         ( ( node % _k ) == _k - 1 ) ) {
      edge = true;
    }
    node /= _k;
      }

      if ( edge ) {
    fail_nodes[i] = true;
      } else {
    fail_nodes[i] = false;
      }
    }

    for ( int i = 0; i < num_fails; ++i ) {
      int j = RandomInt( _size - 1 );
      bool available = false;
      int node, chan;
      int t;

      for ( t = 0; ( t < _size ) && (!available); ++t ) {
    node = ( j + t ) % _size;
       
    if ( !fail_nodes[node] ) {
      // check neighbors
      int c = RandomInt( 2*_n - 1 );

      for ( int n = 0; ( n < 2*_n ) && (!available); ++n ) {
        chan = ( n + c ) % 2*_n;

        if ( chan % 1 ) {
          available = fail_nodes[_LeftNode( node, chan/2 )];
        } else {
          available = fail_nodes[_RightNode( node, chan/2 )];
        }
      }
    }
    
    if ( !available ) {
      cout << "skipping " << node << endl;
    }
      }

      if ( t == _size ) {
    Error( "Could not find another possible fault channel" );
      }

      
      OutChannelFault( node, chan );
      fail_nodes[node] = true;

      for ( int n = 0; ( n < _n ) && available ; ++n ) {
    fail_nodes[_LeftNode( node, n )]  = true;
    fail_nodes[_RightNode( node, n )] = true;
      }

      cout << "failure at node " << node << ", channel " 
       << chan << endl;
    }

    RestoreRandomState( save_x, save_u );
  }
}

double KNCube::Capacity( ) const
{
  return (double)_k / ( _mesh ? 8.0 : 4.0 );
}

int KNCube::GetDirectionOfDest(int src, int dest) {
    int src_x = src % _k;
    int src_y = src / _k;
    int dest_x = dest % _k;
    int dest_y = dest / _k;
    int delta_x = dest_x - src_x;
    int delta_y = dest_y - src_y;

    // if the x delta is bigger than the y delta,
    // we consider this biased in the x direction
    if (ABS(delta_x) >= ABS(delta_y)) {

        // if the delta is positive, it is in the east direction
        if (delta_x >= 0 ) {
            return EAST;
        }

        // the delta is negative, it is in the west direction
        else {
            return WEST;
        }
    }
    
    // y delta bigger than x delta, therefore it is
    // biased in the y direction
    else {

        // if the delta is positive, it is in the south direction
        if (delta_y >= 0 ) {
            return SOUTH;
        }

        // the delta is negative, it is in the north direction
        else {
            return NORTH;
        }
    }
}

int KNCube::GetDistance(int src, int dest) {
    int src_x = src % _k;
    int src_y = src / _k;
    int dest_x = dest % _k;
    int dest_y = dest % _k;
    int delta_x = dest_x - src_x;
    int delta_y = dest_y - src_y;
    int distance = ABS(delta_x) + ABS(delta_y);

    return distance;
}

// NOTE: while we do a linear search here in simulation,
// a linear search is not viable in hardware. one possible
// implementation is to implement a per cache bank decoder
// which, given a direction and set, returns the destination
// node id. unsure how much logic this would take
int KNCube::GetSetBankInDir(int src, int dir, unsigned long addr) {
    assert(NORTH == dir || SOUTH == dir
        || EAST == dir || WEST == dir);

    int min = 999999; // just some outrageously large number
    int closestNode = -1;

    // loop through all nodes in the network to find the
    // closest cache which handles the set in question
    int set = this->getSet(addr);
    for (int i = 0; i < _cacheSets[set].size(); i++) {
        // don't count the source node
        if (i == src) {
            continue;
        }

        int dest = _cacheSets[set][i];
        if (GetDirectionOfDest(src, dest) == dir &&
            GetDistance(src, dest) < min) {
            min = GetDistance(src, dest);
            closestNode = dest;
        }
    }

    return closestNode;
}

int KNCube::GetSaturated(int src, unsigned long addr) {
    int hTendency = _routers[src]->GetHorizontalTendency(addr);
    int vTendency = _routers[src]->GetVerticalTendency(addr);

    if (this->GetSetBankInDir(src, NORTH, addr) != -1 &&
        vTendency == 0){
        return NORTH;
    }
    else if (this->GetSetBankInDir(src, SOUTH, addr) != -1 &&
        vTendency == 3) {
        return SOUTH;
    }
    else if (this->GetSetBankInDir(src, EAST, addr) != -1 &&
        hTendency == 0) {
        return EAST;
    }
    else if (this->GetSetBankInDir(src, WEST, addr) != -1 &&
        hTendency == 3) {
        return WEST;
    }

    return -1;
}

bool KNCube::UpdateTendency(int src, unsigned long addr, int dir) {
    if (this->GetSaturated(src, addr) != -1) {
        return false;
    }

    this->_routers[src]->UpdateTendency(addr, dir);

    if (this->GetSaturated(src, addr) != -1) {
        return true;
    }
    return false;
}
