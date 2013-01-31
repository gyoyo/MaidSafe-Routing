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

#include "maidsafe/routing/processed_messages.h"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ProcessedMessagesTest, BEH_AddRemove) {
  ProcessedMessages processed_messages;
  EXPECT_TRUE(processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32()));
  EXPECT_EQ(processed_messages.history_.size(), 1);

  auto element(processed_messages.history_.at(0));

  EXPECT_FALSE(processed_messages.Add(std::get<0>(element), std::get<1>(element)));

  while (processed_messages.history_.size()  + 1 < Parameters::message_history_cleanup_factor)
    processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32());

  EXPECT_EQ(processed_messages.history_.size() + 1, Parameters::message_history_cleanup_factor);

  Sleep(boost::posix_time::seconds(Parameters::message_age_to_drop + 1));

  EXPECT_TRUE(processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32()));
  element = processed_messages.history_.back();

  EXPECT_EQ(processed_messages.history_.size(), 1);
  EXPECT_EQ(std::get<0>(element), std::get<0>(processed_messages.history_.at(0)));
}


TEST(ProcessedMessagesTest, BEH_Comparison2) {
  std::vector<std::pair<NodeId, int32_t>> vector;
  NodeId node_id;
  uint32_t random;
  for (auto index(0); index < 200; ++index) {
    std::cout << "." << std::flush;
    for (auto in(0); in < 100; ++in) {
      random = RandomUint32();
      node_id = NodeId(NodeId::kRandomId);
      if (std::find_if(vector.begin(),
                       vector.end(),
                       [&](const std::pair<NodeId, int32_t>& pair) {
                         return ((pair.first == node_id) && (pair.second));
                       }) == vector.end()) {
        vector.push_back(std::make_pair(node_id, random));
        if (vector.size() > 1000)
          vector.erase(vector.begin(), vector.begin() + 1);
      }
    }
    Sleep(boost::posix_time::milliseconds(500));
  }
}

TEST(ProcessedMessagesTest, BEH_Comparison) {
  ProcessedMessages processed_messages;
  for (auto index(0); index < 200; ++index) {
      std::cout << "." << std::flush;
    for (auto in(0); in < 100; ++in) {
      processed_messages.Add((NodeId(NodeId::kRandomId)), RandomUint32());
    }
    Sleep(boost::posix_time::milliseconds(500));
  }
}


}  // namespace test

}  // namespace routing

}  // namespace maidsafe

