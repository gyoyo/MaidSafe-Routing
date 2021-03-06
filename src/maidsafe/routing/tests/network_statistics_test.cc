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

#include <bitset>
#include <memory>
#include <numeric>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/group_matrix.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

TEST(NetworkStatisticsTest, BEH_AverageDistance) {
  NodeId node_id(NodeId::kRandomId);
  NodeId average(node_id);
  NetworkStatistics network_statistics(node_id);
  network_statistics.network_distance_data_.average_distance = average;
  network_statistics.UpdateNetworkAverageDistance(average);
  EXPECT_EQ(network_statistics.network_distance_data_.average_distance, average);

  node_id = NodeId();
  network_statistics.network_distance_data_.total_distance = crypto::BigInt::Zero();
  network_statistics.network_distance_data_.average_distance = NodeId();
  average = node_id;
  network_statistics.UpdateNetworkAverageDistance(node_id);
  EXPECT_EQ(network_statistics.network_distance_data_.average_distance, average);

  node_id = NodeId(NodeId::kMaxId);
  network_statistics.network_distance_data_.total_distance =
      crypto::BigInt((node_id.ToStringEncoded(NodeId::kHex) + 'h').c_str()) *
      network_statistics.network_distance_data_.contributors_count;
  average = node_id;
  network_statistics.UpdateNetworkAverageDistance(node_id);
  EXPECT_EQ(network_statistics.network_distance_data_.average_distance, average);

  network_statistics.network_distance_data_.contributors_count = 0;
  network_statistics.network_distance_data_.total_distance = crypto::BigInt::Zero();

  std::vector<NodeId> distances_as_node_id;
  std::vector<crypto::BigInt> distances_as_bigint;
  uint32_t kCount(RandomUint32() % 1000 + 9000);
  for (uint32_t i(0); i < kCount; ++i) {
    NodeId node_id(NodeId::kRandomId);
    distances_as_node_id.push_back(node_id);
    distances_as_bigint.push_back(
        crypto::BigInt((node_id.ToStringEncoded(NodeId::kHex) + 'h').c_str()));
  }

  crypto::BigInt total(std::accumulate(distances_as_bigint.begin(),
                                       distances_as_bigint.end(),
                                       crypto::BigInt::Zero()));

  for (const auto& node_id : distances_as_node_id)
    network_statistics.UpdateNetworkAverageDistance(node_id);

  crypto::BigInt matrix_average_as_bigint(
      (network_statistics.network_distance_data_.average_distance.ToStringEncoded(
          NodeId::kHex) + 'h').c_str());

  EXPECT_EQ(total / kCount, matrix_average_as_bigint);
}

TEST(NetworkStatisticsTest, BEH_IsIdInGroupRange) {
  NodeId node_id;
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeId> nodes_id;
  NodeInfo node_info;
  NodeId my_node(routing_table.kNodeId());
  while (static_cast<uint16_t>(routing_table.size()) <
             Parameters::max_routing_table_size) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    routing_table.group_matrix_.unique_nodes_.push_back(node);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  NodeId info_id(NodeId::kRandomId);
  std::partial_sort(nodes_id.begin(),
                   nodes_id.begin() + Parameters::node_group_size + 1,
                   nodes_id.end(),
                   [&](const NodeId& lhs, const NodeId& rhs) {
                     return NodeId::CloserToTarget(lhs, rhs, info_id);
                   });
  uint16_t index(0);
  while (index < Parameters::max_routing_table_size) {
    if ((nodes_id.at(index) ^ info_id) <= (network_statistics.distance_))
      EXPECT_TRUE(network_statistics.EstimateInGroup(nodes_id.at(index++), info_id));
    else
      EXPECT_FALSE(network_statistics.EstimateInGroup(nodes_id.at(index++), info_id));
  }
}


}  // namespace test
}  // namespace routing
}  // namespace maidsafe


