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

#include "maidsafe/routing/service.h"

#include <vector>

#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/log.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"

namespace maidsafe {

namespace routing {

namespace service {

void Ping(Message &message) {
  protobuf::PingResponse ping_response;
  protobuf::PingRequest ping_request;

  if (!ping_request.ParseFromString(message.Data())) {
    LOG(kError) << "No Data";
    return;
  }
  ping_response.set_pong(true);
  ping_response.set_original_request(message.Data());
 // ping_response.set_original_signature(message.ignature());
  ping_response.set_timestamp(GetTimeStamp());
  message.SetType(-1);
  message.SetData(ping_response.SerializeAsString());
  message.SetDestination(message.SourceId().String());
  message.SetMeAsSource();
  assert(message.Valid() && "unintialised message");
}

void Connect(RoutingTable &routing_table,
             rudp::ManagedConnections &rudp,
             Message &message) {
  protobuf::ConnectRequest connect_request;
  protobuf::ConnectResponse connect_response;
  if (!connect_request.ParseFromString(message.Data()))
    return;  // no need to reply
  NodeInfo node;
  node.node_id = NodeId(connect_request.contact().node_id());
  if (connect_request.bootstrap()) {
             // Already connected
             return;  // FIXME
  }
  connect_response.set_answer(false);
  rudp::EndpointPair our_endpoint;
  Endpoint their_public_endpoint;
  Endpoint their_private_endpoint;
  their_public_endpoint.address(
      boost::asio::ip::address::from_string(connect_request.contact().public_endpoint().ip()));
  their_public_endpoint.port(
      static_cast<unsigned short>(connect_request.contact().public_endpoint().port()));
  their_private_endpoint.address(
      boost::asio::ip::address::from_string(connect_request.contact().private_endpoint().ip()));
  their_private_endpoint.port(
      static_cast<unsigned short>(connect_request.contact().private_endpoint().port()));
  rudp.GetAvailableEndpoint(our_endpoint);

  // TODO(dirvine) try both connections
//  if (message.client_node()) {
//    connect_response.set_answer(true);
//    // TODO(dirvine): get the routing pointer back again
////     node_validation_functor_(routing_table.kKeys().identity,
////                     their_endpoint,
////                     message.client_node(),
////                     our_endpoint);
//  }
//  if ((routing_table.CheckNode(node)) /*&& (!message.client_node()*/) {
//    connect_response.set_answer(true);
////     node_validation_functor_(routing_table.kKeys().identity,
////                     their_endpoint,
////                     message.client_node(),
////                     our_endpoint);
//  }

  protobuf::Contact *contact;
  protobuf::Endpoint *private_endpoint;
  protobuf::Endpoint *public_endpoint;
  contact = connect_response.mutable_contact();
  private_endpoint = contact->mutable_private_endpoint();
  private_endpoint->set_ip(our_endpoint.local.address().to_string());
  private_endpoint->set_port(our_endpoint.local.port());
  public_endpoint = contact->mutable_public_endpoint();
  public_endpoint->set_ip(our_endpoint.local.address().to_string());
  public_endpoint->set_port(our_endpoint.local.port());
  contact->set_node_id(routing_table.kKeys().identity);
  connect_response.set_timestamp(GetTimeStamp());
  connect_response.set_original_request(message.Data());
  connect_response.set_original_signature(message.Signature());
  message.SetDestination(message.SourceId().String());
  message.SetMeAsSource();
  message.SetData(connect_response.SerializeAsString());
  message.SetDirect(ConnectType::kSingle);
  message.SetType(-2);
  assert(message.Valid() && "unintialised message");
}

void FindNodes(RoutingTable &routing_table, Message &message) {
  protobuf::FindNodesRequest find_nodes;
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId>
      nodes(routing_table.GetClosestNodes(message.DestinationId(),
            static_cast<uint16_t>(find_nodes.num_nodes_requested())));
  for (auto it = nodes.begin(); it != nodes.end(); ++it)
    found_nodes.add_nodes((*it).String());
  if (routing_table.Size() < Parameters::closest_nodes_size)
    found_nodes.add_nodes(routing_table.kKeys().identity);  // small network send our ID
  found_nodes.set_original_request(message.Data());
  found_nodes.set_original_signature(message.Signature());
  found_nodes.set_timestamp(GetTimeStamp());
  assert(found_nodes.IsInitialized() && "unintialised found_nodes response");
  message.SetDestination(message.SourceId().String());
  message.SetMeAsSource();
  message.SetData(found_nodes.SerializeAsString());
  message.SetDirect(ConnectType::kSingle);
  message.SetType(-3);
  assert(message.Valid() && "unintialised message");
}

void ProxyConnect(RoutingTable &routing_table,
                  rudp::ManagedConnections &/*rudp*/,
                  Message &message) {

  protobuf::ProxyConnectResponse proxy_connect_response;
  protobuf::ProxyConnectRequest proxy_connect_request;

  if (!proxy_connect_request.ParseFromString(message.Data())) {
    LOG(kError) << "No Data";
    return;
  }

  // TODO(Prakash): any validation needed?

  Endpoint endpoint(boost::asio::ip::address::from_string(proxy_connect_request.endpoint().ip()),
                    static_cast<uint16_t> (proxy_connect_request.endpoint().port()));
  if (routing_table.AmIConnectedToEndpoint(endpoint)) {  // If endpoint already in routing table
    proxy_connect_response.set_result(protobuf::kAlreadyConnected);
  } else {
    bool connect_result(false);
    // TODO(Prakash):  connect_result = rudp.TryConnect(endpoint);
    if (connect_result)
      proxy_connect_response.set_result(protobuf::kSuccess);
    else
      proxy_connect_response.set_result(protobuf::kFailure);
  }
  message.SetType(-4);
  message.SetData(proxy_connect_response.SerializeAsString());
  message.SetDestination(message.SourceId().String());
  message.SetMeAsSource();
  assert(message.Valid() && "unintialised message");
}

}  // namespace service

}  // namespace routing

}  // namespace maidsafe
