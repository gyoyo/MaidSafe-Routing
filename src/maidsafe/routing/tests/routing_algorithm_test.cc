/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include <bitset>
#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

struct RTNode {
  NodeId node_id;
  std::vector<NodeId> close_nodes;
  std::vector<NodeId> accounts;
};

class Network {
 public:
  Network() {
    out_file.open("log.txt");
  }

  void Add(const NodeId& node_id);
  void AddAccount(const NodeId& account);

  void PruneNetwork();
  void PruneAccounts(const NodeId& node_id);
  bool RemovePeer(const NodeId& node_id, const NodeId& requester);
  RTNode MakeNode(const NodeId& node_id);
  void UpdateNetwork(RTNode& new_node);
  void UpdateAccounts(RTNode& new_node);
  bool IsResponsibleForAccount(const RTNode& node, const NodeId& account);
  bool IsResponsibleForAccountMatrix(const RTNode& node, const NodeId& account,
                                     std::vector<NodeId>& node_ids);
  bool Validate();
  void ValidateRoutingTable();
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number);
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number,
                                          std::vector<RTNode>& nodes);
  void RemoveAccount(const RTNode& node, const NodeId& account);
  void PrintNetworkInfo();
  std::vector<NodeId> GetMatrix(const NodeId& node_id);

  void IdealUpdateAccounts(RTNode& new_node);
  void IdealRemoveAccount(const NodeId& account);
  size_t CheckIfAccountHoldersAreConnected(const NodeId& account);
  std::vector<size_t> CheckGroupMatrixReliablity();
  size_t CheckGroupMatrixReliablityForRandomAccounts();

  std::fstream out_file;
  std::vector<RTNode> nodes_;
  std::vector<NodeId> accounts_;
};

void Network::Add(const NodeId& node_id) {
  auto node = MakeNode(node_id);
  UpdateNetwork(node);
  UpdateAccounts(node);
  nodes_.push_back(node);
  PruneNetwork();
  PruneAccounts(node.node_id);
  LOG(kInfo) << "Added NodeId : " << DebugId(node_id);
}

RTNode Network::MakeNode(const NodeId &node_id) {
  PartialSortFromTarget(node_id, 8);
  RTNode node;
  node.node_id = node_id;
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    node.close_nodes.push_back(nodes_[i].node_id);
    LOG(kInfo) << DebugId(node.node_id) << " added " << DebugId(nodes_[i].node_id);
  }
  return node;
}

void Network::UpdateNetwork(RTNode& new_node) {
  PartialSortFromTarget(new_node.node_id, nodes_.size());
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    nodes_[i].close_nodes.push_back(new_node.node_id);
    LOG(kVerbose) << DebugId(nodes_[i].node_id)
                  << " network added "
                  << DebugId(new_node.node_id);
  }

  NodeId node_id;
  for (size_t index(8); index < nodes_.size(); ++index) {
    node_id = nodes_[index].node_id;
    std::sort(nodes_[index].close_nodes.begin(),
              nodes_[index].close_nodes.end(),
              [node_id](const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, node_id);
              });
    if (NodeId::CloserToTarget(new_node.node_id,
                               nodes_[index].close_nodes[7],
                               nodes_[index].node_id)) {
      LOG(kVerbose) << DebugId(nodes_[index].node_id)
                    << " network added "
                    << DebugId(new_node.node_id);
      new_node.close_nodes.push_back(nodes_[index].node_id);
      nodes_[index].close_nodes.push_back(new_node.node_id);
    }
  }
}

void Network::UpdateAccounts(RTNode& new_node) {
  std::vector<NodeId> node_ids;
  std::vector<RTNode> rt_nodes;
  size_t close_nodes_size;

  PartialSortFromTarget(new_node.node_id, 8);

  for (size_t i(0); (i < 8) && i < nodes_.size(); ++i) {
    if (std::find(node_ids.begin(), node_ids.end(), nodes_[i].node_id) == node_ids.end())
      node_ids.push_back(nodes_[i].node_id);

    close_nodes_size = std::min(size_t(8), nodes_[i].close_nodes.size());
    for (auto itr(nodes_[i].close_nodes.begin());
         itr != nodes_[i].close_nodes.begin() + close_nodes_size;
         ++itr) {
      if (std::find(node_ids.begin(), node_ids.end(), *itr) == node_ids.end() &&
          (*itr != new_node.node_id)) {
        node_ids.push_back(*itr);
      }
    }
  }

  for (auto& node_id : node_ids) {
    auto itr(std::find_if(nodes_.begin(),
                          nodes_.end(),
                          [node_id] (const RTNode& rt_node) {
                            return rt_node.node_id == node_id;
                          }));
    assert(itr != nodes_.end());
    rt_nodes.push_back(*itr);
  }

  std::vector<NodeId> accounts;
  for (auto& node : rt_nodes) {
    accounts = node.accounts;
    for (auto& account : accounts) {
      if (IsResponsibleForAccountMatrix(new_node, account, node_ids)) {
         if (std::find(new_node.accounts.begin(),
                       new_node.accounts.end(),
                       account) == new_node.accounts.end()) {
           new_node.accounts.push_back(account);
           LOG(kInfo) << DebugId(new_node.node_id) << " << added account >>" << DebugId(account);
         }
      }
    }
  }
}


bool Network::IsResponsibleForAccountMatrix(const RTNode& node, const NodeId& account,
                                            std::vector<NodeId>& node_ids) {
  if (node_ids.size() < 4)
    return true;
  std::sort(node_ids.begin(),
            node_ids.end(),
            [account](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, account);
            });
  return NodeId::CloserToTarget(node.node_id, node_ids[3], account);
}

bool Network::IsResponsibleForAccount(const RTNode& node, const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  if (count < 4)
    return true;
  return NodeId::CloserToTarget(node.node_id, nodes_[3].node_id, account);
}

bool Network::RemovePeer(const NodeId& node_id, const NodeId& requester) {
  auto node(std::find_if(nodes_.begin(),
                         nodes_.end(),
                         [node_id] (const RTNode& rt_node) {
                           return (rt_node.node_id == node_id);
                         }));
  std::sort(node->close_nodes.begin(),
            node->close_nodes.end(),
            [node](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, node->node_id);
            });
  auto peer_itr(std::find(node->close_nodes.begin(),
                          node->close_nodes.end(),
                          requester));
  if (std::distance(node->close_nodes.begin(), peer_itr) > 8) {
    LOG(kInfo) << DebugId(node->node_id) << " removes peer " << DebugId(*peer_itr);
    node->close_nodes.erase(peer_itr);
    return true;
  }
  return false;
}

void Network::AddAccount(const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  for (uint16_t index(0); index != count; ++index) {
    nodes_.at(index).accounts.push_back(account);
    LOG(kInfo) << DebugId(nodes_.at(index).node_id) << " added " <<  DebugId(account);
  }
  accounts_.push_back(account);
}

void Network::PruneAccounts(const NodeId& node_id) {
  std::vector<NodeId> matrix, accounts, node_ids;
  std::vector<RTNode> nodes;
  std::string log;

  node_ids = GetMatrix(node_id);

  for (auto& node : nodes_) {
    if (std::find(node_ids.begin(), node_ids.end(), node.node_id) == node_ids.end())
      continue;
    matrix = GetMatrix(node.node_id);
    if (matrix.size() < 4)
      return;

    accounts = node.accounts;
    NodeId account;
    for (auto itr(accounts.begin()); itr != accounts.end(); ++itr) {
      account = (*itr);
      std::sort(matrix.begin(),
                matrix.end(),
                [account] (const NodeId& lhs, const NodeId& rhs) {
                  return NodeId::CloserToTarget(lhs, rhs, account);
                });
      if (NodeId::CloserToTarget(matrix[3], node.node_id, account)) {
        auto remove(std::find(node.accounts.begin(),
                              node.accounts.end(), account));
        assert(remove != node.accounts.end());
        node.accounts.erase(remove);
        LOG(kInfo) << DebugId(node.node_id) << " removed account " << DebugId(*remove);
      }
    }
  }
}

void Network::PruneNetwork() {
  for (auto& node : nodes_) {
    if (node.close_nodes.size() <= 8)
      continue;
    std::sort(node.close_nodes.begin(),
              node.close_nodes.end(),
              [node](const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, node.node_id);
              });
    for (size_t index(8); index < node.close_nodes.size(); ++index) {
      if (RemovePeer(node.close_nodes[index], node.node_id)) {
          node.close_nodes.erase(std::remove(node.close_nodes.begin(),
                                             node.close_nodes.end(),
                                             node.close_nodes[index]), node.close_nodes.end());
          LOG(kInfo) << DebugId(node.close_nodes[index])
                     << " and " << DebugId(node.node_id) << " removed each other ";
      }
    }
  }
}

void Network::IdealUpdateAccounts(RTNode& new_node) {
  std::vector<NodeId> accounts;
  std::vector<RTNode> nodes;
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    nodes.push_back(nodes_[i]);
  }
  for (auto& node : nodes) {
    accounts = node.accounts;
    for (auto& account : accounts) {
      if (IsResponsibleForAccount(new_node, account)) {
         if (std::find(new_node.accounts.begin(),
                  new_node.accounts.end(),
                  account) == new_node.accounts.end())
            new_node.accounts.push_back(account);
          IdealRemoveAccount(account);
      }
    }
  }
}

void Network::IdealRemoveAccount(const NodeId& account) {
  if (nodes_.size() > 3)
    nodes_[3].accounts.erase(std::remove_if(nodes_[3].accounts.begin(),
                                          nodes_[3].accounts.end(),
                                          [account] (const NodeId& node_id) {
                                            return (node_id == account);
                                          }), nodes_[3].accounts.end());
}


void Network::RemoveAccount(const RTNode& node, const NodeId& account) {
  auto holder(std::find_if(nodes_.begin(),
                           nodes_.end(),
                           [node] (const RTNode& rt_node) {
                             return (rt_node.node_id == node.node_id);
                           }));
  assert(holder != nodes_.end());
  if (nodes_.size() > 3 && nodes_[3].node_id == holder->node_id) {
    holder->accounts.erase(std::remove_if(holder->accounts.begin(),
                                        holder->accounts.end(),
                                        [account] (const NodeId& node_id) {
                                          return (node_id == account);
                                        }), holder->accounts.end());
    LOG(kInfo) << DebugId(holder->node_id) << " removed account " << DebugId(account);
  }
}

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes_.size()));
  std::partial_sort(nodes_.begin(),
                    nodes_.begin() + count,
                    nodes_.end(),
                    [target](const RTNode& lhs, const RTNode& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
  return count;
}

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number,
                                        std::vector<RTNode>& nodes) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes.size()));
  std::partial_sort(nodes.begin(),
                    nodes.begin() + count,
                    nodes.end(),
                    [target](const RTNode& lhs, const RTNode& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
  return count;
}

std::vector<NodeId> Network::GetMatrix(const NodeId& node_id) {
  std::vector<NodeId> return_nodes;
  NodeId local_id;
  size_t size;

  auto nodes_copy = nodes_;
  PartialSortFromTarget(node_id, 9, nodes_copy);
  for (size_t i(1); (i < 9) && i < nodes_copy.size(); ++i) {
    if (std::find(return_nodes.begin(),
                  return_nodes.end(),
                  nodes_copy[i].node_id) == return_nodes.end())
      return_nodes.push_back(nodes_copy[i].node_id);
    local_id = (nodes_copy[i].node_id);
    std::sort(nodes_copy[i].close_nodes.begin(),
              nodes_copy[i].close_nodes.end(),
              [local_id] (const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, local_id);
              });
    size = std::min(size_t(8), nodes_copy[i].close_nodes.size());
    for (auto itr(nodes_copy[i].close_nodes.begin());
        itr != nodes_copy[i].close_nodes.begin() + size;
        ++itr) {
      if (*itr == node_id)
        continue;
      if (std::find(return_nodes.begin(), return_nodes.end(), *itr) == return_nodes.end())
        return_nodes.push_back(*itr);
    }
  }
  return return_nodes;
}


bool Network::Validate() {
  size_t max_holers(0), extra_holers(0), max_disconnected_holders(0), disconnected_holders(0),
      total_disconnected_holders(0);
  for (auto& account : accounts_) {
    size_t count(std::count_if(nodes_.begin(),
                               nodes_.end(),
                               [account] (const RTNode& node) {
                                 return std::find(node.accounts.begin(),
                                                  node.accounts.end(),
                                                  account) != node.accounts.end();
                  }));
    std::partial_sort(nodes_.begin(),
                      nodes_.begin() + count,
                      nodes_.end(),
                      [account](const RTNode& lhs, const RTNode& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, account);
                      });
    disconnected_holders = CheckIfAccountHoldersAreConnected(account);
    max_disconnected_holders = std::max(max_disconnected_holders, disconnected_holders);
    total_disconnected_holders += disconnected_holders;
    if (count >= 5) {
      extra_holers++;
      max_holers = std::max(max_holers, count);
//      LOG(kInfo) << "Account " << DebugId(account) << " # of holders: " << count;
//      for (size_t index(4); index < count; ++index)
//        LOG(kInfo) << DebugId(nodes_.at(index).node_id)
//                   << " Wrong holder of : " << DebugId(account);
    }
    for (auto itr(nodes_.begin()); itr != nodes_.begin() + 4; ++itr) {
      EXPECT_NE(std::find(itr->accounts.begin(), itr->accounts.end(), account),
                itr->accounts.end()) << "Node: " << DebugId(itr->node_id)
                                     << " does not have " << DebugId(account);
    }
  }
  LOG(kInfo) << "# of account hold by more than 4 holders: " << extra_holers
             << " which is " << extra_holers * 100.0 / accounts_.size() << "% of acconts";
  LOG(kInfo) << "Maximum holders for an account is: " << max_holers;
  LOG(kInfo) << "# of disconnected holders " << total_disconnected_holders;
  LOG(kInfo) << "Maximum # of disconnected holders " << max_disconnected_holders;
  ValidateRoutingTable();
  return true;
}

void Network::ValidateRoutingTable() {
  std::vector<NodeId> node_ids;
  for (auto& node : nodes_)
    node_ids.push_back(node.node_id);
  for (auto& node : nodes_) {
    std::partial_sort(node_ids.begin(),
                      node_ids.begin() + 9,
                      node_ids.end(),
                      [node] (const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, node.node_id);
                      });
    std::sort(node.close_nodes.begin(),
              node.close_nodes.end(),
              [node] (const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, node.node_id);
              });
    for (size_t index(1); index < 8; ++index)  {
      EXPECT_NE(std::find(node.close_nodes.begin(),
                          node.close_nodes.end(),
                          node_ids[index]), node.close_nodes.end())
          << DebugId(node.node_id) << " should have "
          << DebugId(node_ids[index]) << " in RT ";
    }
  }
}


void Network::PrintNetworkInfo() {
  size_t max_close_nodes_size(0), min_close_nodes_size(6400), max_accounts_size(0),
      min_matrix_size(100), max_matrix_size(0), avg_matrix_size(0);
  std::vector<size_t> group_matrix_miss;
  std::vector<NodeId> matrix;
  std::vector<RTNode> rt_nodes(nodes_);

  for (auto& node : rt_nodes) {
    matrix = GetMatrix(node.node_id);
    LOG(kInfo) << "Size of matrix for: " << DebugId(node.node_id) << " is " << matrix.size();
    min_matrix_size = std::min(min_matrix_size, matrix.size());
    max_matrix_size = std::max(max_matrix_size, matrix.size());
    avg_matrix_size += matrix.size();
    LOG(kInfo) <<  DebugId(node.node_id)
                << ", closests: " << node.close_nodes.size()
                << ", accounts: " << node.accounts.size();
    max_close_nodes_size = std::max(max_close_nodes_size, node.close_nodes.size());
    min_close_nodes_size = std::min(min_close_nodes_size, node.close_nodes.size());
    max_accounts_size = std::max(max_accounts_size, node.accounts.size());
  }
  group_matrix_miss = CheckGroupMatrixReliablity();
  LOG(kInfo) <<  "Maximum close nodes size: " <<  max_close_nodes_size;
  LOG(kInfo) <<  "Minimum close nodes size: " <<  min_close_nodes_size;
  LOG(kInfo) <<  "Maximum account size: " <<  max_accounts_size;
  LOG(kInfo) <<  "Maximum matrix size: " <<  max_matrix_size;
  LOG(kInfo) <<  "Minimum matrix size: " <<  min_matrix_size;
  LOG(kInfo) <<  "Average matrix size: " <<  avg_matrix_size / nodes_.size();
  for (size_t index(0); index < 4; ++index) {
    LOG(kInfo) <<  "Number of times matrix missing required holders for existing accounts on "
               << index << "th closet node " << group_matrix_miss.at(index);
  }
  LOG(kInfo) <<  "Number of accounts in the network " << accounts_.size();
//  LOG(kInfo) <<  "Number of times matrix missing required holders for random accounts "
//             << CheckGroupMatrixReliablityForRandomAccounts();
}

std::vector<size_t> Network::CheckGroupMatrixReliablity() {
  std::vector<size_t> little_matrix(4, 0);
  std::vector<NodeId> matrix;
  for (auto& account : accounts_) {
    std::partial_sort(nodes_.begin(),
                      nodes_.begin() + 5,
                      nodes_.end(),
                     [account] (const RTNode& lhs, const RTNode& rhs) {
                       return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, account);
                     });
    for (size_t node_index(0); node_index < 4; ++node_index) {
      matrix = GetMatrix(nodes_[node_index].node_id);
      for (size_t index(0); index < 4; ++index) {
        if (index == node_index)
          continue;
        if (std::find(matrix.begin(),
                      matrix.end(),
                      nodes_[index].node_id) == matrix.end()) {
          LOG(kInfo) << "Matrix of " << DebugId(nodes_[0].node_id) << " does not have "
                     << DebugId(nodes_[index].node_id) << " as a holder of account "
                     << DebugId(account);
          little_matrix[index]++;
        }
      }
    }
  }
  return little_matrix;
}

size_t Network::CheckGroupMatrixReliablityForRandomAccounts() {
  size_t little_matrix(0), iteration(0);
  std::vector<NodeId> matrix;
  NodeId random_account;
  do {
    random_account = NodeId(NodeId::kRandomId);
    std::partial_sort(nodes_.begin(),
                      nodes_.begin() + 5,
                      nodes_.end(),
                      [random_account] (const RTNode& lhs, const RTNode& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, random_account);
                      });
    matrix = GetMatrix(nodes_[0].node_id);
    for (size_t index(1); index < 4; ++index) {
      if (std::find(matrix.begin(),
                    matrix.end(),
                    nodes_[index].node_id) == matrix.end()) {
        LOG(kInfo) << "Matrix of " << DebugId(nodes_[0].node_id) << " does not have "
                   << DebugId(nodes_[index].node_id) << " as a holder of account "
                   << DebugId(random_account);
        little_matrix++;
      }
    }
  } while (iteration++ < 2000);
  return little_matrix;
}


size_t Network::CheckIfAccountHoldersAreConnected(const NodeId& account) {
  size_t disconnected_holders(0);
  for (size_t i(0); i < 4; ++i) {
    for (size_t j(i + 1); j < 4; ++j) {
      if (i == j)
        continue;
      if (std::find(nodes_[i].close_nodes.begin(),
                    nodes_[i].close_nodes.end(),
                    nodes_[j].node_id) == nodes_[i].close_nodes.end()) {
        LOG(kInfo) << DebugId(nodes_[i].node_id) << " and " << DebugId(nodes_[j].node_id)
                   << " are holders of " << DebugId(account) << " but they are not connected";
        ++disconnected_holders;
      }
    }
  }
  return disconnected_holders;
}


TEST(RoutingTableTest, BEH_RT) {
  Network network;
  for (auto i(0); i != 500; ++i) {
    network.Add(NodeId(NodeId::kRandomId));
    if (i % 5 == 0)
      network.AddAccount(NodeId(NodeId::kRandomId));
    LOG(kInfo) << "Iteration # " << i;
  }
  network.PrintNetworkInfo();
  network.Validate();
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
