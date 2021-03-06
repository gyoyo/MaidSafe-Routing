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

#include "maidsafe/routing/group_matrix.h"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <set>

#include "maidsafe/common/log.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"

namespace maidsafe {

namespace routing {

GroupMatrix::GroupMatrix(const NodeId& this_node_id, bool client_mode)
    : kNodeId_(this_node_id),
      unique_nodes_(),
      client_mode_(client_mode),
      matrix_() {}

void GroupMatrix::AddConnectedPeer(const NodeInfo& node_info) {
  LOG(kVerbose) << DebugId(kNodeId_) << " AddConnectedPeer : " << DebugId(node_info.node_id);
  auto node_id(node_info.node_id);
  auto found(std::find_if(matrix_.begin(),
                          matrix_.end(),
                          [node_id] (const std::vector<NodeInfo>& info) {
                            return info.begin()->node_id == node_id;
                          }));
  if (found != matrix_.end()) {
    LOG(kWarning) << "Already Added in matrix";
    return;
  }
  matrix_.push_back(std::vector<NodeInfo>(1, node_info));
  UpdateUniqueNodeList();
}

void GroupMatrix::RemoveConnectedPeer(const NodeInfo& node_info, MatrixChange& matrix_change) {
  matrix_change.old_matrix = GetUniqueNodeIds();
  matrix_.erase(std::remove_if(matrix_.begin(),
                               matrix_.end(),
                               [node_info](const std::vector<NodeInfo> nodes) {
                                 return (node_info.node_id == nodes.begin()->node_id);
                               }), matrix_.end());
  Prune();
  UpdateUniqueNodeList();
  matrix_change.new_matrix = GetUniqueNodeIds();
}

std::vector<NodeInfo> GroupMatrix::GetConnectedPeers() const {
  std::vector<NodeInfo> connected_peers;
  for (const auto& nodes : matrix_) {
    if (nodes.begin()->node_id != kNodeId_)
      connected_peers.push_back(nodes.at(0));
  }
  return connected_peers;
}

NodeInfo GroupMatrix::GetConnectedPeerFor(const NodeId& target_node_id) {
/*
  for (const auto& nodes : matrix_) {
    if (nodes.at(0).node_id == target_node_id) {
      assert(false && "Shouldn't request connected peer for node in first column of group matrix.");
      return NodeInfo();
    }
  }*/
  for (const auto& nodes : matrix_) {
    if (std::find_if(nodes.begin(), nodes.end(),
                     [target_node_id](const NodeInfo& node_info) {
                       return (node_info.node_id == target_node_id);
                     }) != nodes.end()) {
      return nodes.at(0);
    }
  }
  return NodeInfo();
}

void GroupMatrix::GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                                 const std::vector<std::string>& exclude,
                                                 bool ignore_exact_match,
                                                 NodeInfo& current_closest_peer) {
  NodeId closest_id(current_closest_peer.node_id);

  for (const auto& row : matrix_) {
    if (ignore_exact_match && row.at(0).node_id == target_node_id)
      continue;
    if (std::find(exclude.begin(), exclude.end(), row.at(0).node_id.string()) != exclude.end())
      continue;

    for (const auto& node : row) {
      if (node.node_id == kNodeId_)
        continue;
      if (ignore_exact_match && node.node_id == target_node_id)
        continue;
      if (std::find(exclude.begin(), exclude.end(), node.node_id.string()) != exclude.end())
        continue;
      if (NodeId::CloserToTarget(node.node_id, closest_id, target_node_id)) {
        closest_id = node.node_id;
        current_closest_peer = row.at(0);
      }
    }
  }
  LOG(kVerbose) << "[" << DebugId(kNodeId_)
                << "]\ttarget: " << DebugId(target_node_id)
                << "\tfound node in matrix: " << DebugId(closest_id)
                << "\treccommend sending to: " << DebugId(current_closest_peer.node_id);
}

void GroupMatrix::GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                                 bool ignore_exact_match,
                                                 NodeId& current_closest_peer_id) {
  NodeId closest_id(current_closest_peer_id);

  for (const auto& row : matrix_) {
    if (ignore_exact_match && row.at(0).node_id == target_node_id)
      continue;

    for (const auto& node : row) {
      if (ignore_exact_match && node.node_id == target_node_id)
        continue;
      if (NodeId::CloserToTarget(node.node_id, closest_id, target_node_id)) {
        closest_id = node.node_id;
        current_closest_peer_id = row.at(0).node_id;
      }
    }
  }
  LOG(kVerbose) << "[" << DebugId(kNodeId_)
                << "]\ttarget: " << DebugId(target_node_id)
                << "\tfound node in matrix: " << DebugId(closest_id)
                << "\treccommend sending to: " << DebugId(current_closest_peer_id);
}

std::vector<NodeInfo> GroupMatrix::GetAllConnectedPeersFor(const NodeId& target_id) {
  std::vector<NodeInfo> connected_nodes;
  for (const auto& row : matrix_) {
    if (std::find_if(row.begin(),
                     row.end(),
                     [&target_id] (const NodeInfo& node_info) {
                       return target_id == node_info.node_id;
                     }) != row.end()) {
      connected_nodes.push_back(row.at(0));
    }
  }
  return connected_nodes;
}

bool GroupMatrix::IsThisNodeGroupLeader(const NodeId& target_id, NodeId& connected_peer) {
  assert(!client_mode_ && "Client should not call IsThisNodeGroupLeader.");
  if (client_mode_)
    return false;

  LOG(kVerbose) << " Destination " << DebugId(target_id) << " kNodeId " << DebugId(kNodeId_);
  bool is_group_leader = true;
  if (unique_nodes_.empty()) {
    is_group_leader = true;
    return true;
  }

  std::string log("unique_nodes_ for " + DebugId(kNodeId_) + " are ");
  for (const auto& node : unique_nodes_) {
    log += DebugId(node.node_id) + ", ";
  }
  LOG(kVerbose) << log;

  for (const auto& node : unique_nodes_) {
    if (node.node_id == target_id)
      continue;
    if (NodeId::CloserToTarget(node.node_id, kNodeId_, target_id)) {
      LOG(kVerbose) << DebugId(node.node_id) << " could be leader";
      is_group_leader = false;
      break;
    }
  }
  if (!is_group_leader) {
    NodeId better_id(kNodeId_);
    GetBetterNodeForSendingMessage(target_id, true, better_id);
    connected_peer = better_id;
    assert(connected_peer != target_id);
  }
  return is_group_leader;
}

bool GroupMatrix::ClosestToId(const NodeId& target_id) {
  if (unique_nodes_.size() == 0)
    return true;

  PartialSortFromTarget(target_id, 2, unique_nodes_);
  if (unique_nodes_.at(0).node_id == kNodeId_)
    return true;

  if (unique_nodes_.at(0).node_id == target_id) {
    if (unique_nodes_.at(1).node_id == kNodeId_)
      return true;
    else
      return NodeId::CloserToTarget(kNodeId_, unique_nodes_.at(1).node_id, target_id);
  }

  return NodeId::CloserToTarget(kNodeId_, unique_nodes_.at(0).node_id, target_id);
}

bool GroupMatrix::IsNodeIdInGroupRange(const NodeId& target_id) {
  if (unique_nodes_.size() < Parameters::node_group_size) {
    return true;
  }

  PartialSortFromTarget(kNodeId_, Parameters::node_group_size, unique_nodes_);

  NodeInfo furthest_group_node(unique_nodes_.at(Parameters::node_group_size - 1));
  return !NodeId::CloserToTarget(furthest_group_node.node_id, target_id, kNodeId_);
}

void GroupMatrix::UpdateFromConnectedPeer(const NodeId& peer,
                                          const std::vector<NodeInfo>& nodes) {
  assert(nodes.size() < Parameters::max_routing_table_size);
  if (peer.IsZero()) {
    assert(false && "Invalid peer node id.");
    return;
  }
  // If peer is in my group
  auto group_itr(matrix_.begin());
  for (; group_itr != matrix_.end(); ++group_itr) {
    if (group_itr->begin()->node_id == peer)
      break;
  }

  if (group_itr == matrix_.end()) {
    LOG(kWarning) << "Peer Node : " << DebugId(peer)
                  << " is not in closest group of this node.";
    return;
  }

  // Update peer's row
  if (group_itr->size() > 1) {
    group_itr->erase(group_itr->begin() + 1, group_itr->end());
  }
  for (const auto& i : nodes)
    group_itr->push_back(i);

  // Update unique node vector
  Prune();
  UpdateUniqueNodeList();
}

bool GroupMatrix::GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries) {
  if (row_id.IsZero()) {
    assert(false && "Invalid node id.");
    return false;
  }
  auto group_itr(matrix_.begin());
  for (group_itr = matrix_.begin(); group_itr != matrix_.end(); ++group_itr) {
    if ((*group_itr).at(0).node_id == row_id)
      break;
  }

  if (group_itr == matrix_.end())
    return false;

  row_entries.clear();
  for (uint32_t i(0); i < (*group_itr).size(); ++i) {
    if (i != 0) {
      row_entries.push_back((*group_itr).at(i));
    }
  }
  return true;
}

std::vector<NodeInfo> GroupMatrix::GetUniqueNodes() const {
  return unique_nodes_;
}

std::vector<NodeId> GroupMatrix::GetUniqueNodeIds() const {
  std::vector<NodeId> unique_node_ids;
  for (auto& node_info : unique_nodes_)
    unique_node_ids.push_back(node_info.node_id);
  return unique_node_ids;
}

bool GroupMatrix::IsRowEmpty(const NodeInfo& node_info) {
  auto group_itr(matrix_.begin());
  for (group_itr = matrix_.begin(); group_itr != matrix_.end(); ++group_itr) {
    if ((*group_itr).at(0).node_id == node_info.node_id)
      break;
  }
  assert(group_itr != matrix_.end());
  if (group_itr == matrix_.end())
    return false;

  return (group_itr->size() < 2);
}

std::vector<NodeInfo> GroupMatrix::GetClosestNodes(const uint16_t& size) {
  uint16_t size_to_sort(std::min(size, static_cast<uint16_t>(unique_nodes_.size())));
  PartialSortFromTarget(kNodeId_, size_to_sort, unique_nodes_);
  return std::vector<NodeInfo>(unique_nodes_.begin(), unique_nodes_.begin() + size_to_sort);
}

bool GroupMatrix::Contains(const NodeId& node_id) {
  return std::find_if(unique_nodes_.begin(), unique_nodes_.end(),
                      [&node_id] (const NodeInfo& node_info) {
                        return node_info.node_id == node_id;
                      }) != unique_nodes_.end();
}

void GroupMatrix::UpdateUniqueNodeList() {
  std::set<NodeInfo, std::function<bool(const NodeInfo&, const NodeInfo&)>> sorted_to_owner(
      [&](const NodeInfo& lhs, const NodeInfo& rhs) {
          return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
      });
  if (!client_mode_) {
    NodeInfo node_info;
    node_info.node_id = kNodeId_;
    sorted_to_owner.insert(node_info);
  }
  for (const auto& node_ids : matrix_) {
    for (const auto& node_id : node_ids)
      sorted_to_owner.insert(node_id);
  }

  unique_nodes_.assign(std::begin(sorted_to_owner), std::end(sorted_to_owner));
}

void GroupMatrix::PartialSortFromTarget(const NodeId& target,
                                        const uint16_t& number,
                                        std::vector<NodeInfo>& nodes) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes.size()));
  std::partial_sort(nodes.begin(),
                    nodes.begin() + count,
                    nodes.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
}

void GroupMatrix::Prune() {
  if (matrix_.size() <= Parameters::closest_nodes_size)
    return;
  NodeId node_id;
  std::vector<NodeId> peers_to_remove;
  std::partial_sort(matrix_.begin(),
                    matrix_.begin() + Parameters::closest_nodes_size,
                    matrix_.end(),
                    [this](const std::vector<NodeInfo>& lhs, const std::vector<NodeInfo>& rhs) {
                      return NodeId::CloserToTarget(lhs.begin()->node_id,
                                                    rhs.begin()->node_id,
                                                    kNodeId_);
                    });
  auto itr(matrix_.begin());
  std::advance(itr, Parameters::closest_nodes_size);
  for (; itr != matrix_.end(); ++itr) {
    if (client_mode_) {
      peers_to_remove.push_back(itr->begin()->node_id);
      continue;
    }
    node_id = itr->begin()->node_id;
    if (itr->size() < Parameters::closest_nodes_size) {
      peers_to_remove.push_back(node_id);
      continue;
    }
    std::partial_sort(itr->begin() + 1,
                      itr->begin() + Parameters::closest_nodes_size + 1,
                      itr->end(),
                      [node_id] (const NodeInfo& lhs, const NodeInfo& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, node_id);
                      });
    if (NodeId::CloserToTarget(itr->at(Parameters::closest_nodes_size).node_id, kNodeId_, node_id))
      peers_to_remove.push_back(node_id);
  }
  for (auto& peer : peers_to_remove) {
    LOG(kInfo) << DebugId(kNodeId_) << " matrix conected removes " << DebugId(peer);
    matrix_.erase(std::remove_if(matrix_.begin(),
                                 matrix_.end(),
                                 [peer] (const std::vector<NodeInfo>& row) {
                                   return row.begin()->node_id == peer;
                                 }));
  }
}

void GroupMatrix::PrintGroupMatrix() {
  auto group_itr(matrix_.begin());
  std::string tab("\t");
  std::string output("Group matrix of node with NodeID: " + DebugId(kNodeId_));
  for (group_itr = matrix_.begin(); group_itr != matrix_.end(); ++group_itr) {
    output.append("\nGroup matrix row:");
    for (uint32_t i(0); i < (*group_itr).size(); ++i) {
      output.append(tab);
      output.append(DebugId((*group_itr).at(i).node_id));
    }
  }
  LOG(kVerbose) << output;
}

}  // namespace routing

}  // namespace maidsafe
