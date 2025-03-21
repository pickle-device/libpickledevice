// Copyright (c) 2015, The Regents of the University of California (Regents)
// See LICENSE.txt for license details

#ifndef GRAPH_H_
#define GRAPH_H_

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include "util.h"
#include "pickle_job.h"

#include <iostream>

/*
GAP Benchmark Suite
Class:  CSRGraph
Author: Scott Beamer

Simple container for graph in CSR format
 - Intended to be constructed by a Builder
 - To make weighted, set DestID_ template type to NodeWeight
 - MakeInverse parameter controls whether graph stores its inverse
*/

// Used to hold node & weight, with another node it makes a weighted edge
template <typename NodeID_, typename WeightT_>
struct NodeWeight {
  NodeID_ v;
  WeightT_ w;
  NodeWeight() {}
  NodeWeight(NodeID_ v) : v(v), w(1) {}
  NodeWeight(NodeID_ v, WeightT_ w) : v(v), w(w) {}

  bool operator< (const NodeWeight& rhs) const {
    return v == rhs.v ? w < rhs.w : v < rhs.v;
  }

  // doesn't check WeightT_s, needed to remove duplicate edges
  bool operator== (const NodeWeight& rhs) const {
    return v == rhs.v;
  }

  // doesn't check WeightT_s, needed to remove self edges
  bool operator== (const NodeID_& rhs) const {
    return v == rhs;
  }

  operator NodeID_() {
    return v;
  }
};

template <typename NodeID_, typename WeightT_>
std::ostream& operator<<(std::ostream& os,
                         const NodeWeight<NodeID_, WeightT_>& nw) {
  os << nw.v << " " << nw.w;
  return os;
}

template <typename NodeID_, typename WeightT_>
std::istream& operator>>(std::istream& is, NodeWeight<NodeID_, WeightT_>& nw) {
  is >> nw.v >> nw.w;
  return is;
}



// Syntatic sugar for an edge
template <typename SrcT, typename DstT = SrcT>
struct EdgePair {
  SrcT u;
  DstT v;

  EdgePair() {}

  EdgePair(SrcT u, DstT v) : u(u), v(v) {}

  bool operator< (const EdgePair& rhs) const {
    return u == rhs.u ? v < rhs.v : u < rhs.u;
  }

  bool operator== (const EdgePair& rhs) const {
    return (u == rhs.u) && (v == rhs.v);
  }
};

// SG = serialized graph, these types are for writing graph to file
typedef int32_t SGID;
typedef EdgePair<SGID> SGEdge;
typedef int64_t SGOffset;



template <class NodeID_, class DestID_ = NodeID_, bool MakeInverse = true>
class CSRGraph {
  // Used for *non-negative* offsets within a neighborhood
  typedef std::make_unsigned<std::ptrdiff_t>::type OffsetT;

  // Used to access neighbors of vertex, basically sugar for iterators
  class Neighborhood {
    NodeID_ n_;
    DestID_** g_index_;
    OffsetT start_offset_;
   public:
    Neighborhood(NodeID_ n, DestID_** g_index, OffsetT start_offset) :
        n_(n), g_index_(g_index), start_offset_(0) {
      OffsetT max_offset = end() - begin();
      start_offset_ = std::min(start_offset, max_offset);
//      std::cout << "Neighborhood idx 0 " << std::hex << g_index_[0] << std::dec << "\n";
    }
    typedef DestID_* iterator;
    iterator begin() { return g_index_[n_] + start_offset_; }
    iterator end()   { return g_index_[n_+1]; }
  };

  void ReleaseResources() {
    if (out_index_ != nullptr)
      delete[] out_index_;
    if (out_neighbors_ != nullptr)
      delete[] out_neighbors_;
    if (directed_) {
      if (in_index_ != nullptr)
        delete[] in_index_;
      if (in_neighbors_ != nullptr)
        delete[] in_neighbors_;
    }
    in_index_array_descriptor = nullptr;
    out_index_array_descriptor = nullptr;
    in_neighbors_array_descriptor = nullptr;
    out_neighbors_array_descriptor = nullptr;
  }

  void constructArrayDescriptors() {
    // in_index array
    in_index_array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
    const AddressRange inIndexAddressRange = getInIndexAddressRange();
    in_index_array_descriptor->vaddr_start = inIndexAddressRange.start;
    in_index_array_descriptor->vaddr_end = inIndexAddressRange.end;
    in_index_array_descriptor->element_size = getInIndexElementSize();

    // out_index array
    out_index_array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
    const AddressRange outIndexAddressRange = getOutIndexAddressRange();
    out_index_array_descriptor->vaddr_start = outIndexAddressRange.start;
    out_index_array_descriptor->vaddr_end = outIndexAddressRange.end;
    out_index_array_descriptor->element_size = getOutIndexElementSize();

    // in_neighbors array
    in_neighbors_array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
    const AddressRange inNeighborsAddressRange = getInNeighborsAddressRange();
    in_neighbors_array_descriptor->vaddr_start = inNeighborsAddressRange.start;
    in_neighbors_array_descriptor->vaddr_end = inNeighborsAddressRange.end;
    in_neighbors_array_descriptor->element_size = getInNeighborsElementSize();

    // out_neighbors array
    out_neighbors_array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
    const AddressRange outNeighborsAddressRange = getOutNeighborsAddressRange();
    out_neighbors_array_descriptor->vaddr_start = outNeighborsAddressRange.start;
    out_neighbors_array_descriptor->vaddr_end = outNeighborsAddressRange.end;
    out_neighbors_array_descriptor->element_size = getOutNeighborsElementSize();

    constructArrayRelations();
  }

  void constructArrayRelations() {
    this->inNeighborsIndexedBy(this->getInIndexArrayDescriptor());
    this->outNeighborsIndexedBy(this->getOutIndexArrayDescriptor());
  }

 public:
  CSRGraph() : directed_(false), num_nodes_(-1), num_edges_(-1),
    out_index_(nullptr), out_neighbors_(nullptr),
    in_index_(nullptr), in_neighbors_(nullptr) {}

  CSRGraph(int64_t num_nodes, DestID_** index, DestID_* neighs) :
    directed_(false), num_nodes_(num_nodes),
    out_index_(index), out_neighbors_(neighs),
    in_index_(index), in_neighbors_(neighs) {
      num_edges_ = (out_index_[num_nodes_] - out_index_[0]) / 2;
      constructArrayDescriptors();
    }

  CSRGraph(int64_t num_nodes, DestID_** out_index, DestID_* out_neighs,
        DestID_** in_index, DestID_* in_neighs) :
    directed_(true), num_nodes_(num_nodes),
    out_index_(out_index), out_neighbors_(out_neighs),
    in_index_(in_index), in_neighbors_(in_neighs) {
      num_edges_ = out_index_[num_nodes_] - out_index_[0];
      constructArrayDescriptors();
//      std::cout << "ctor CSRGraph out_index_[0] " << std::hex << out_index_[0] << std::dec << "\n";
    }

  CSRGraph(CSRGraph&& other) : directed_(other.directed_),
    num_nodes_(other.num_nodes_), num_edges_(other.num_edges_),
    out_index_(other.out_index_), out_neighbors_(other.out_neighbors_),
    in_index_(other.in_index_), in_neighbors_(other.in_neighbors_),
    in_index_array_descriptor(other.in_index_array_descriptor),
    out_index_array_descriptor(other.out_index_array_descriptor),
    in_neighbors_array_descriptor(other.in_neighbors_array_descriptor),
    out_neighbors_array_descriptor(other.out_neighbors_array_descriptor) {
      other.num_edges_ = -1;
      other.num_nodes_ = -1;
      other.out_index_ = nullptr;
      other.out_neighbors_ = nullptr;
      other.in_index_ = nullptr;
      other.in_neighbors_ = nullptr;
  }

  ~CSRGraph() {
    ReleaseResources();
  }

  CSRGraph& operator=(CSRGraph&& other) {
    if (this != &other) {
      ReleaseResources();
      directed_ = other.directed_;
      num_edges_ = other.num_edges_;
      num_nodes_ = other.num_nodes_;
      out_index_ = other.out_index_;
      out_neighbors_ = other.out_neighbors_;
      in_index_ = other.in_index_;
      in_neighbors_ = other.in_neighbors_;
      in_index_array_descriptor = other.in_index_array_descriptor;
      out_index_array_descriptor = other.out_index_array_descriptor;
      in_neighbors_array_descriptor = other.in_neighbors_array_descriptor;
      out_neighbors_array_descriptor = other.out_neighbors_array_descriptor;
      other.num_edges_ = -1;
      other.num_nodes_ = -1;
      other.out_index_ = nullptr;
      other.out_neighbors_ = nullptr;
      other.in_index_ = nullptr;
      other.in_neighbors_ = nullptr;
      other.in_index_array_descriptor = nullptr;
      other.out_index_array_descriptor = nullptr;
      other.in_neighbors_array_descriptor = nullptr;
      other.out_neighbors_array_descriptor = nullptr;
    }
    return *this;
  }

  bool directed() const {
    return directed_;
  }

  int64_t num_nodes() const {
    return num_nodes_;
  }

  int64_t num_edges() const {
    return num_edges_;
  }

  int64_t num_edges_directed() const {
    return directed_ ? num_edges_ : 2*num_edges_;
  }

  int64_t out_degree(NodeID_ v) const {
    return out_index_[v+1] - out_index_[v];
  }

  int64_t in_degree(NodeID_ v) const {
    static_assert(MakeInverse, "Graph inversion disabled but reading inverse");
    return in_index_[v+1] - in_index_[v];
  }

  Neighborhood out_neigh(NodeID_ n, OffsetT start_offset = 0) const {
    return Neighborhood(n, out_index_, start_offset);
  }

  Neighborhood in_neigh(NodeID_ n, OffsetT start_offset = 0) const {
    static_assert(MakeInverse, "Graph inversion disabled but reading inverse");
    return Neighborhood(n, in_index_, start_offset);
  }

  void PrintStats() const {
    std::cout << "Graph has " << num_nodes_ << " nodes and "
              << num_edges_ << " ";
    if (!directed_)
      std::cout << "un";
    std::cout << "directed edges for degree: ";
    std::cout << num_edges_/num_nodes_ << std::endl;
  }

  void PrintTopology() const {
    for (NodeID_ i=0; i < num_nodes_; i++) {
      std::cout << i << ": ";
      for (DestID_ j : out_neigh(i)) {
        std::cout << j << " ";
      }
      std::cout << std::endl;
    }
  }

  static DestID_** GenIndex(const pvector<SGOffset> &offsets, DestID_* neighs) {
    NodeID_ length = offsets.size();
    DestID_** index = new DestID_*[length];
//    std::cout << "PM:PM GenIndex index data start addr " << std::hex << index<< std::dec << " size " << length << "\n\n";
    #pragma omp parallel for
    for (NodeID_ n=0; n < length; n++)
      index[n] = neighs + offsets[n];
    return index;
  }

  pvector<SGOffset> VertexOffsets(bool in_graph = false) const {
    pvector<SGOffset> offsets(num_nodes_+1);
    for (NodeID_ n=0; n < num_nodes_+1; n++)
      if (in_graph)
        offsets[n] = in_index_[n] - in_index_[0];
      else
        offsets[n] = out_index_[n] - out_index_[0];
    return offsets;
  }

  Range<NodeID_> vertices() const {
    return Range<NodeID_>(num_nodes());
  }

  // -------------------- Array descriptor interface --------------------
  AddressRange getOutIndexAddressRange() const {
      return AddressRange((uint64_t)out_index_, (uint64_t)(out_index_ + num_nodes_ + 1));
  }
  uint64_t getOutIndexElementSize() const {
      return sizeof(DestID_*);
  };
  std::shared_ptr<PickleArrayDescriptor> getOutIndexArrayDescriptor() const {
    return this->out_index_array_descriptor;
  }
  void outIndexIndexedBy(const std::shared_ptr<PickleArrayDescriptor>& descriptor) const {
    descriptor->dst_indexing_array_id = this->out_index_array_descriptor->getArrayId();
  }

  AddressRange getInIndexAddressRange() const {
      return AddressRange((uint64_t)in_index_, (uint64_t)(in_index_ + num_nodes_ + 1));
  }
  uint64_t getInIndexElementSize() const {
      return sizeof(DestID_*);
  }
  std::shared_ptr<PickleArrayDescriptor> getInIndexArrayDescriptor() const {
    return this->in_index_array_descriptor;
  }
  void inIndexIndexedBy(const std::shared_ptr<PickleArrayDescriptor>& descriptor) const {
    descriptor->dst_indexing_array_id = this->in_index_array_descriptor->getArrayId();
  }

  AddressRange getOutNeighborsAddressRange() const {
      return AddressRange((uint64_t)out_neighbors_, (uint64_t)(out_neighbors_ + num_edges_));
  }
  uint64_t getOutNeighborsElementSize() const {
      return sizeof(DestID_);
  }
  std::shared_ptr<PickleArrayDescriptor> getOutNeighborsArrayDescriptor() const {
    return this->out_neighbors_array_descriptor;
  }
  void outNeighborsIndexedBy(const std::shared_ptr<PickleArrayDescriptor>& descriptor) const {
    descriptor->dst_indexing_array_id = this->out_neighbors_array_descriptor->getArrayId();
  }

  AddressRange getInNeighborsAddressRange() const {
      return AddressRange((uint64_t)in_neighbors_, (uint64_t)(in_neighbors_ + num_edges_));
  }
  uint64_t getInNeighborsElementSize() const {
      return sizeof(DestID_);
  }  
  std::shared_ptr<PickleArrayDescriptor> getInNeighborsArrayDescriptor() const {
    return this->in_neighbors_array_descriptor;
  }
  void inNeighborsIndexedBy(const std::shared_ptr<PickleArrayDescriptor>& descriptor) const {
    descriptor->dst_indexing_array_id = this->in_neighbors_array_descriptor->getArrayId();
  }
  // -------------------------- Interface END ---------------------------

  // out_index_ contains pointer to out_neighbors. To save space, write out indices instead. The consumer must convert it back to ptr.
  void WriteOutIndex(std::ofstream &f, const std::string& msg) const {
      f << msg << " " << num_nodes_+1 << "\n";
      DestID_*  base_addr = out_neighbors_;
      for (auto i = 0; i <= num_nodes_; ++i) {
          // pointer arithmetric take size of entry into account already
          f << (out_index_[i] - base_addr)  << "\n";
      }
  }

  void WriteInIndex(std::ofstream &f, const std::string& msg) const {
      f << msg << " " << num_nodes_+1 << "\n";
      DestID_*  base_addr = in_neighbors_;
      for (auto i = 0; i <= num_nodes_; ++i) {
          // pointer arithmetric take size of entry into account already
          f << (in_index_[i] - base_addr)  << "\n";
      }
  }


  void WriteOutNeigh(std::ofstream &f, const std::string& msg) const {
  //    f << "oneigh " << std::hex << out_neighbors_ << " " << out_neighbors_ + num_edges_ << " " << std::dec << num_edges_ << "\n";
      f << msg << " " << num_edges_ << "\n";
      int count = 0;
      for (auto i = 0; i < num_nodes_; ++i) {
          for (auto n : out_neigh(i)) {
              // the format dictate the index start form 0 not 1
              f << n << "\n";
              count++;
          }
      }
      if (count != num_edges_) {
          std::cout << "WriteOutNeigh found wrong number of edges. " << count << " instead of " << num_edges_ << "\n";
      }
  }

  void WriteInNeigh(std::ofstream &f, const std::string& msg) const {
  //    f << "ineigh " << std::hex << in_neighbors_ << " " << in_neighbors_ + num_edges_ << " " << std::dec << num_edges_ << "\n";
      f << msg << " " << num_edges_ << "\n";
      int count = 0;
      for (auto i = 0; i < num_nodes_; ++i) {
          for (auto n : in_neigh(i)) {
              // the format dictate the index start form 0 not 1
              f << n << "\n";
              count++;
          }
      }
      if (count != num_edges_) {
          std::cout << "WriteInNeigh found wrong number of edges. " << count << " instead of " << num_edges_ << "\n";
      }
  }


 private:
  bool directed_;
  int64_t num_nodes_;
  int64_t num_edges_;
  DestID_** out_index_;
  DestID_*  out_neighbors_;
  DestID_** in_index_;
  DestID_*  in_neighbors_;
  // Array Descriptor
  std::shared_ptr<PickleArrayDescriptor> in_index_array_descriptor;
  std::shared_ptr<PickleArrayDescriptor> out_index_array_descriptor;
  std::shared_ptr<PickleArrayDescriptor> in_neighbors_array_descriptor;
  std::shared_ptr<PickleArrayDescriptor> out_neighbors_array_descriptor;

 public:
  void printGraph() {
    for (int64_t node_id = 0; node_id < num_nodes_; node_id++) {
        std::cout << "Node_id: " << node_id << std::endl;
        std::cout << "    out_index_ptr 0x" << std::hex << (uint64_t)(out_index_[node_id]) << std::dec;
        uint64_t count = 0;
        for (DestID_* ptr = out_index_[node_id]; ptr < out_index_[node_id+1]; ptr++) {
            if (count % 10 == 0)
                std::cout << std::endl << "        ";
            std::cout << ptr[0] << " ";
            count++;
        }
        std::cout << std::endl;
    }
  }
};


// Make non-member to specialize easier.
// Write graph out in .graph format.
// The filename must not have extension. .graph will be prepended.
void WriteGraph(const CSRGraph<int, NodeWeight<int,int>, true> &g, std::string fname) {
    // first line: num_node num_edge weight_indicator 14 53 1
    // each line afterword represent one node in index order (start from index 1)
    // Within each line contains a list of neighbor index and edge weight.
    // This line shows a node with 2 neighbors, ie., 2 and 3.
    // The width to node 2 is 19, while that to node 3 is 5
    // 2 19 3 5

    std::ofstream gfile(fname);
    gfile << g.num_nodes() << " " << g.num_edges() << " 1\n";
    for (auto i = 0; i < g.num_nodes(); ++i) {
        for (auto n : g.out_neigh(i)) {
            // the format dictate the index start form 1 not 0
            gfile << (n.v)+1 << " " << n.w << " ";
        }
        gfile << "\n";
    }
    gfile.close();
}

void WriteGraph(const CSRGraph<int, int, true> &g, std::string fname) {
    // first line: num_node num_edge weight_indicator 14 53 1
    // each line afterword represent one node in index order (start from index 1)
    // Within each line contains a list of neighbor index and edge weight.
    // This line shows a node with 2 neighbors, ie., 2 and 3.
    // The width to node 2 is 19, while that to node 3 is 5
    // 2 19 3 5

    std::ofstream gfile(fname);
    gfile << g.num_nodes() << " " << g.num_edges() << " 0\n";
    for (auto i = 0; i < g.num_nodes(); ++i) {
        for (auto n : g.out_neigh(i)) {
            // the format dictate the index start form 1 not 0
            gfile << n+1 << " ";
        }
        gfile << "\n";
    }
    gfile.close();
}

#endif  // PICKLE_GRAPH_H_
