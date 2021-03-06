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

#include "maidsafe/routing/tests/mock_response_handler.h"

namespace maidsafe {

namespace routing {

namespace test {

MockResponseHandler::MockResponseHandler(RoutingTable& routing_table,
                         ClientRoutingTable& client_routing_table,
                         NetworkUtils& utils,
                         GroupChangeHandler &group_change_handler)
    : ResponseHandler(routing_table, client_routing_table, utils, group_change_handler) {}

MockResponseHandler::~MockResponseHandler() {}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

