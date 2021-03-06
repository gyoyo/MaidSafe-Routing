/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/client_routing_table.h"

#include "maidsafe/common/log.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/parameters.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ClientRoutingTable::ClientRoutingTable(const NodeId& node_id)
    : kNodeId_(node_id),
      nodes_(),
      mutex_() {}

bool ClientRoutingTable::AddNode(NodeInfo& node, const NodeId& furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, true);
}

bool ClientRoutingTable::CheckNode(NodeInfo& node, const NodeId& furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, false);
}

bool ClientRoutingTable::AddOrCheckNode(NodeInfo& node,
                                     const NodeId& furthest_close_node_id,
                                     const bool& add) {
  if (node.node_id == kNodeId_)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (CheckRangeForNodeToBeAdded(node, furthest_close_node_id, add)) {
    if (add) {
      nodes_.push_back(node);
      LOG(kInfo) << "Added to ClientRoutingTable :" << DebugId(node.node_id);
      LOG(kVerbose) << PrintClientRoutingTable();
    }
    return true;
  }
  return false;
}

std::vector<NodeInfo> ClientRoutingTable::DropNodes(const NodeId &node_to_drop) {
  std::vector<NodeInfo> nodes_info;
  std::lock_guard<std::mutex> lock(mutex_);
  uint16_t i(0);
  while (i < nodes_.size()) {
    if (nodes_.at(i).node_id == node_to_drop) {
      nodes_info.push_back(nodes_.at(i));
      nodes_.erase(nodes_.begin() + i);
    } else {
      ++i;
    }
  }
  return nodes_info;
}

NodeInfo ClientRoutingTable::DropConnection(const NodeId& connection_to_drop) {
  NodeInfo node_info;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).connection_id == connection_to_drop) {
      node_info = *it;
      nodes_.erase(it);
      break;
    }
  }
  return node_info;
}

std::vector<NodeInfo> ClientRoutingTable::GetNodesInfo(const NodeId& node_id) const {
  std::vector<NodeInfo> nodes_info;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).node_id == node_id)
      nodes_info.push_back(*it);
  }
  return nodes_info;
}

bool ClientRoutingTable::Contains(const NodeId& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::find_if(nodes_.begin(),
                      nodes_.end(),
                      [node_id](const NodeInfo& node_info) {
                        return node_info.node_id == node_id;
                      }) != nodes_.end();
}

bool ClientRoutingTable::IsConnected(const NodeId& node_id) const {
  return Contains(node_id);
}

size_t ClientRoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// TODO(Prakash): re-order checks to increase performance if needed
bool ClientRoutingTable::CheckValidParameters(const NodeInfo& node) const {
  // bucket index is not used in ClientRoutingTable
  if (node.bucket != NodeInfo::kInvalidBucket) {
    LOG(kInfo) << "Invalid bucket index.";
    return false;
  }
  return CheckParametersAreUnique(node);
}

bool ClientRoutingTable::CheckParametersAreUnique(const NodeInfo& node) const {
  // If we already have a duplicate endpoint return false
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [node](const NodeInfo& node_info) {
                     return (node_info.connection_id == node.connection_id);
                   }) != nodes_.end()) {
    LOG(kInfo) << "Already have node with this connection_id.";
    return false;
  }

  // If we already have a duplicate public key under different node ID return false
//  if (std::find_if(nodes_.begin(),
//                   nodes_.end(),
//                   [node](const NodeInfo& node_info) {
//                     return (asymm::MatchingKeys(node_info.public_key, node.public_key) &&
//                             (node_info.node_id != node.node_id));
//                   }) != nodes_.end()) {
//    LOG(kInfo) << "Already have a different node ID with this public key.";
//    return false;
//  }
  return true;
}

bool ClientRoutingTable::CheckRangeForNodeToBeAdded(NodeInfo& node,
                                                 const NodeId& furthest_close_node_id,
                                                 const bool& add) const {
  if (nodes_.size() >= Parameters::max_client_routing_table_size) {
    LOG(kInfo) << "ClientRoutingTable full.";
    return false;
  }

  if (add && !CheckValidParameters(node)) {
    LOG(kInfo) << "Invalid Parameters.";
    return false;
  }

  return IsThisNodeInRange(node.node_id, furthest_close_node_id);
}

bool ClientRoutingTable::IsThisNodeInRange(const NodeId& node_id,
                                        const NodeId& furthest_close_node_id) const {
  if (furthest_close_node_id == node_id) {
    assert(false && "node_id (client) and furthest_close_node_id (vault) should not be equal.");
    return false;
  }
  return (furthest_close_node_id ^ kNodeId_) > (node_id ^ kNodeId_);
}

std::string ClientRoutingTable::PrintClientRoutingTable() {
  auto rt(nodes_);
  std::string s = "\n\n[" + DebugId(kNodeId_) +
      "] This node's own ClientRoutingTable and peer connections:\n";
  for (const auto& node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.node_id) + "]"+ "-->";
    s += DebugId(node.connection_id)+ "\n";
  }
  s += "\n\n";
  return s;
}

}  // namespace routing

}  // namespace maidsafe
