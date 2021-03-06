/*
 * Copyright (c) 2017, Matias Fontanini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef CPP_KAFKA_CONSUMER_H
#define CPP_KAFKA_CONSUMER_H

#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include "kafka_handle_base.h"
#include "message.h"
#include "macros.h"
#include "error.h"

namespace cppkafka {

class TopicConfiguration;

/**
 * \brief High level kafka consumer class
 *
 * Wrapper for the high level consumer API provided by rdkafka. Most methods are just
 * a one to one mapping to rdkafka functions.
 *
 * This class allows hooking up to assignments/revocations via callbacks.
 *
 * Semi-simple code showing how to use this class
 *
 * \code
 * // Create a configuration and set the group.id and broker list fields 
 * Configuration config = {
 *     { "metadata.broker.list", "127.0.0.1:9092" },
 *     { "group.id", "foo" }
 * };
 *
 * // Create a consumer
 * Consumer consumer(config);
 *
 * // Set the assignment callback
 * consumer.set_assignment_callback([&](vector<TopicPartition>& topic_partitions) {
 *     // Here you could fetch offsets and do something, altering the offsets on the
 *     // topic_partitions vector if needed
 *     cout << "Got assigned " << topic_partitions.count() << " partitions!" << endl;
 * });
 *
 * // Set the revocation callback
 * consumer.set_revocation_callback([&](const vector<TopicPartition>& topic_partitions) {
 *     cout << topic_partitions.size() << " partitions revoked!" << endl;
 * });
 * 
 * // Subscribe 
 * consumer.subscribe({ "my_topic" });
 * while (true) {
 *     // Poll. This will optionally return a message. It's necessary to check if it's a valid
 *     // one before using it
 *     Message msg = consumer.poll(); 
 *     if (msg) {
 *         if (!msg.get_error()) {
 *             // It's an actual message. Get the payload and print it to stdout
 *             cout << msg.get_payload().as_string() << endl;
 *         }
 *         else if (!msg.is_eof()) {
 *             // Is it an error notification, handle it.
 *             // This is explicitly skipping EOF notifications as they're not actually errors,
 *             // but that's how rdkafka provides them
 *         }
 *     }
 * }
 * \endcode
 */
class CPPKAFKA_API Consumer : public KafkaHandleBase {
public:
    using AssignmentCallback = std::function<void(TopicPartitionList&)>;
    using RevocationCallback = std::function<void(const TopicPartitionList&)>;
    using RebalanceErrorCallback = std::function<void(Error)>;

    /**
     * \brief Creates an instance of a consumer.
     * 
     * Note that the configuration *must contain* the group.id attribute set or this 
     * will throw.
     *
     * \param config The configuration to be used
     */ 
    Consumer(Configuration config);
    Consumer(const Consumer&) = delete;
    Consumer(Consumer&&) = delete;
    Consumer& operator=(const Consumer&) = delete;
    Consumer& operator=(Consumer&&) = delete;

    /**
     * \brief Closes and estroys the rdkafka handle
     *
     * This will call Consumer::close before destroying the handle
     */
    ~Consumer();

    /**
     * \brief Sets the topic/partition assignment callback
     * 
     * The Consumer class will use rd_kafka_conf_set_rebalance_cb and will handle the
     * rebalance, converting from rdkafka topic partition list handles into vector<TopicPartition>
     * and executing the assignment/revocation/rebalance_error callbacks.
     *
     * \note You *do not need* to call Consumer::assign with the provided topic parttitions. This
     * will be handled automatically by cppkafka.
     *
     * \param callback The topic/partition assignment callback
     */
    void set_assignment_callback(AssignmentCallback callback);

    /**
     * \brief Sets the topic/partition revocation callback
     * 
     * The Consumer class will use rd_kafka_conf_set_rebalance_cb and will handle the
     * rebalance, converting from rdkafka topic partition list handles into vector<TopicPartition>
     * and executing the assignment/revocation/rebalance_error callbacks.
     *
     * \note You *do not need* to call Consumer::assign with an empty topic partition list or
     * anything like that. That's handled automatically by cppkafka. This is just a notifitation
     * so your application code can react to revocations
     *
     * \param callback The topic/partition revocation callback
     */
    void set_revocation_callback(RevocationCallback callback);

    /**
     * \brief Sets the rebalance error callback
     * 
     * The Consumer class will use rd_kafka_conf_set_rebalance_cb and will handle the
     * rebalance, converting from rdkafka topic partition list handles into vector<TopicPartition>
     * and executing the assignment/revocation/rebalance_error callbacks.
     *
     * \param callback The rebalance error callback
     */
    void set_rebalance_error_callback(RebalanceErrorCallback callback);

    /**
     * \brief Subscribes to the given list of topics
     *
     * This translates to a call to rd_kafka_subscribe
     *
     * \param topics The topics to subscribe to
     */
    void subscribe(const std::vector<std::string>& topics);

    /**
     * \brief Unsubscribes to the current subscription list
     *
     * This translates to a call to rd_kafka_unsubscribe
     */
    void unsubscribe();

    /**
     * \brief Sets the current topic/partition assignment
     *
     * This translates into a call to rd_kafka_assign
     */
    void assign(const TopicPartitionList& topic_partitions);

    /**
     * \brief Unassigns the current topic/partition assignment
     *
     * This translates into a call to rd_kafka_assign using a null as the topic partition list 
     * parameter
     */ 
    void unassign();

    /**
     * \brief Commits the given message synchronously
     *
     * This translates into a call to rd_kafka_commit_message
     *
     * \param msg The message to be committed
     */
    void commit(const Message& msg);

    /**
     * \brief Commits the given message asynchronously
     *
     * This translates into a call to rd_kafka_commit_message
     *
     * \param msg The message to be committed
     */
    void async_commit(const Message& msg);

    /**
     * \brief Commits the offsets on the given topic/partitions synchronously
     *
     * This translates into a call to rd_kafka_commit
     *
     * \param topic_partitions The topic/partition list to be committed
     */
    void commit(const TopicPartitionList& topic_partitions);

    /**
     * \brief Commits the offsets on the given topic/partitions asynchronously
     *
     * This translates into a call to rd_kafka_commit
     *
     * \param topic_partitions The topic/partition list to be committed
     */
    void async_commit(const TopicPartitionList& topic_partitions);

    /**
     * \brief Gets the minimum and maximum offsets for the given topic/partition
     *
     * This translates into a call to rd_kafka_get_watermark_offsets
     *
     * \param topic_partition The topic/partition to get the offsets from 
     */
    OffsetTuple get_offsets(const TopicPartition& topic_partition) const;

    /**
     * \brief Gets the offsets committed for the given topic/partition list
     *
     * This translates into a call to rd_kafka_committed
     *
     * \param topic_partitions The topic/partition list to be queried
     */
    TopicPartitionList get_offsets_committed(const TopicPartitionList& topic_partitions) const;

    /**
     * \brief Gets the offset positions for the given topic/partition list
     *
     * This translates into a call to rd_kafka_position
     *
     * \param topic_partitions The topic/partition list to be queried
     */
    TopicPartitionList get_offsets_position(const TopicPartitionList& topic_partitions) const;

    /**
     * \brief Gets the current topic subscription
     *
     * This translates to a call to rd_kafka_subscription
     */
    std::vector<std::string> get_subscription() const;

    /**
     * \brief Gets the current topic/partition list assignment
     *
     * This translates to a call to rd_kafka_assignment
     */
    TopicPartitionList get_assignment() const;

    /**
     * \brief Gets the group member id
     *
     * This translates to a call to rd_kafka_memberid
     */
    std::string get_member_id() const;

    /**
     * Gets the partition assignment callback.
     */
    const AssignmentCallback& get_assignment_callback() const;

    /**
     * Gets the partition revocation callback.
     */
    const RevocationCallback& get_revocation_callback() const;

    /**
     * Gets the rebalance error callback.
     */
    const RebalanceErrorCallback& get_rebalance_error_callback() const;

    /**
     * \brief Polls for new messages
     *
     * This will call rd_kafka_consumer_poll.
     *
     * Note that you need to call poll periodically as a keep alive mechanism, otherwise the broker
     * will think this consumer is down and will trigger a rebalance (if using dynamic 
     * subscription).
     *
     * The timeout used on this call will be the one configured via Consumer::set_timeout.
     *
     * The returned message *might* be empty. If's necessary to check that it's a valid one before
     * using it:
     * 
     * \code
     * Message msg = consumer.poll(); 
     * if (msg) {
     *     // It's a valid message!
     * }
     * \endcode
     */
    Message poll();

    /**
     * \brief Polls for new messages
     *
     * Same as the other overload of Consumer::poll but the provided timeout will be used
     * instead of the one configured on this Consumer.
     *
     * \param timeout The timeout to be used on this call
     */
    Message poll(std::chrono::milliseconds timeout);
private:
    static void rebalance_proxy(rd_kafka_t *handle, rd_kafka_resp_err_t error,
                                rd_kafka_topic_partition_list_t *partitions, void *opaque);

    void close();
    void commit(const Message& msg, bool async);
    void commit(const TopicPartitionList& topic_partitions, bool async);
    void handle_rebalance(rd_kafka_resp_err_t err, TopicPartitionList& topic_partitions);

    AssignmentCallback assignment_callback_;
    RevocationCallback revocation_callback_;
    RebalanceErrorCallback rebalance_error_callback_;
};

} // cppkafka

#endif // CPP_KAFKA_CONSUMER_H
