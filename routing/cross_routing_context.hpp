#pragma once

#include "../coding/file_container.hpp"

#include "../geometry/point2d.hpp"

#include "../std/string.hpp"
#include "../std/vector.hpp"

namespace routing
{
using WritedEdgeWeightT = uint32_t;
static WritedEdgeWeightT const INVALID_CONTEXT_EDGE_WEIGHT = std::numeric_limits<WritedEdgeWeightT>::max();
static WritedEdgeWeightT const INVALID_CONTEXT_EDGE_NODE_ID = std::numeric_limits<uint32_t>::max();

struct IngoingCrossNode
{
  uint32_t m_nodeId;
  m2::PointD m_point;

  IngoingCrossNode() : m_nodeId(INVALID_CONTEXT_EDGE_NODE_ID), m_point(m2::PointD::Zero()) {}
  IngoingCrossNode(uint32_t nodeId, m2::PointD const & point) : m_nodeId(nodeId), m_point(point) {}

  void Save(Writer & w) const;

  size_t Load(Reader const & r, size_t pos);
};

struct OutgoingCrossNode
{
  uint32_t m_nodeId;
  m2::PointD m_point;
  unsigned char m_outgoingIndex;

  OutgoingCrossNode() : m_nodeId(INVALID_CONTEXT_EDGE_NODE_ID), m_point(m2::PointD::Zero()), m_outgoingIndex(0) {}
  OutgoingCrossNode(uint32_t nodeId, size_t const index, m2::PointD const & point) : m_nodeId(nodeId),
                                                                                     m_point(point),
                                                                                     m_outgoingIndex(static_cast<unsigned char>(index)){}

  void Save(Writer & w) const;

  size_t Load(Reader const & r, size_t pos);
};

using IngoingEdgeIteratorT = vector<IngoingCrossNode>::const_iterator;
using OutgoingEdgeIteratorT = vector<OutgoingCrossNode>::const_iterator;

/// Reader class from cross context section in mwm.routing file
class CrossRoutingContextReader
{
  vector<IngoingCrossNode> m_ingoingNodes;
  vector<OutgoingCrossNode> m_outgoingNodes;
  vector<string> m_neighborMwmList;
  unique_ptr<Reader> mp_reader = nullptr;

  size_t GetIndexInAdjMatrix(IngoingEdgeIteratorT ingoing, OutgoingEdgeIteratorT outgoing) const;

public:  
  void Load(Reader const & r);

  const string & getOutgoingMwmName(size_t mwmIndex) const;

  pair<IngoingEdgeIteratorT, IngoingEdgeIteratorT> GetIngoingIterators() const;

  pair<OutgoingEdgeIteratorT, OutgoingEdgeIteratorT> GetOutgoingIterators() const;

  WritedEdgeWeightT getAdjacencyCost(IngoingEdgeIteratorT ingoing, OutgoingEdgeIteratorT outgoing) const;
};

/// Helper class to generate cross context section in mwm.routing file
class CrossRoutingContextWriter
{
  vector<IngoingCrossNode> m_ingoingNodes;
  vector<OutgoingCrossNode> m_outgoingNodes;
  vector<WritedEdgeWeightT> m_adjacencyMatrix;
  vector<string> m_neighborMwmList;

  size_t GetIndexInAdjMatrix(IngoingEdgeIteratorT ingoing, OutgoingEdgeIteratorT outgoing) const;

public:
  void Save(Writer & w);

  void addIngoingNode(size_t const nodeId, m2::PointD const & point);

  void addOutgoingNode(size_t const nodeId, string const & targetMwm, m2::PointD const & point);

  void reserveAdjacencyMatrix();

  void setAdjacencyCost(IngoingEdgeIteratorT ingoing, OutgoingEdgeIteratorT outgoin, WritedEdgeWeightT value);

  pair<IngoingEdgeIteratorT, IngoingEdgeIteratorT> GetIngoingIterators() const;

  pair<OutgoingEdgeIteratorT, OutgoingEdgeIteratorT> GetOutgoingIterators() const;
};
}
