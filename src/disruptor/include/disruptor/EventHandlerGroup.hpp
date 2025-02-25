#pragma once

#include <memory>
#include <vector>

#include "ConsumerRepository.hpp"
#include "IEventHandler.hpp"
#include "IEventProcessorFactory.hpp"
#include "Sequence.hpp"

namespace opencmw::disruptor {

/**
 * A group of IEventProcessor's used as part of the Disruptor
 *
 * \tparam T the type of event used by IEventProcessor's.
 */
template<class T, std::size_t SIZE, WaitStrategy WAIT_STRATEGY, template<std::size_t, typename> typename CLAIM_STRATEGY, template<typename, std::size_t, typename, template<std::size_t, typename> typename> typename TDisruptor>
class EventHandlerGroup {
private:
    using EventHandlerGroupType = EventHandlerGroup<T, SIZE, WAIT_STRATEGY, CLAIM_STRATEGY, TDisruptor>;

    std::shared_ptr<TDisruptor<T, SIZE, WAIT_STRATEGY, CLAIM_STRATEGY>> _disruptor;
    std::shared_ptr<ConsumerRepository<T>>                              _consumerRepository;
    std::vector<std::shared_ptr<Sequence>>                              _sequences;

public:
    EventHandlerGroup(const std::shared_ptr<TDisruptor<T, SIZE, WAIT_STRATEGY, CLAIM_STRATEGY>> &disruptor,
            const std::shared_ptr<ConsumerRepository<T>>                                        &consumerRepository,
            const std::vector<std::shared_ptr<Sequence>>                                        &sequences)
        : _disruptor(disruptor)
        , _consumerRepository(consumerRepository)
        , _sequences(sequences) {
    }

    /**
     * Create a new event handler group that combines the consumers in this group with otherHandlerGroup
     *
     * \param otherHandlerGroup the event handler group to combine
     * \returns a new EventHandlerGroup combining the existing and new consumers into a single dependency group
     */
    std::shared_ptr<EventHandlerGroupType> And(const std::shared_ptr<EventHandlerGroupType> &otherHandlerGroup) {
        std::vector<std::shared_ptr<Sequence>> sequences(_sequences);
        std::ranges::copy(otherHandlerGroup->_sequences, std::back_inserter(sequences));

        return std::make_shared<EventHandlerGroupType>(_disruptor, _consumerRepository, sequences);
    }

    /**
     * Create a new event handler group that combines the handlers in this group with processors
     *
     * \param processors the processors to combine
     * \returns a new EventHandlerGroup combining the existing and new processors into a single dependency group
     */
    std::shared_ptr<EventHandlerGroupType> And(const std::vector<std::shared_ptr<IEventProcessor>> &processors) {
        std::vector<std::shared_ptr<Sequence>> sequences;

        for (auto &&eventProcessor : processors) {
            _consumerRepository->add(eventProcessor);
            sequences.push_back(eventProcessor->sequence());
        }

        std::ranges::copy(_sequences, std::back_inserter(sequences));

        return std::make_shared<EventHandlerGroupType>(_disruptor, _consumerRepository, sequences);
    }

    /**
     * Create a new event handler group that combines the handlers in this group with processors
     *
     * \param processor the processor to combine
     * \returns a new EventHandlerGroup combining the existing and new processors into a single dependency group
     */
    std::shared_ptr<EventHandlerGroupType> And(const std::shared_ptr<IEventProcessor> &processor) {
        return And(std::vector<std::shared_ptr<IEventProcessor>>{ processor });
    }

    /**
     * Set up batch handlers to consume events from the ring buffer. These handlers will only process events after every IEventProcessor in this group has processed the event.
     * This method is generally used as part of a chain. For example if the handler A must process events before handler B: dw.handleEventsWith(A).then(B)
     *
     * \param handlers
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> then(const std::vector<std::shared_ptr<IEventHandler<T>>> &handlers) {
        return handleEventsWith(handlers);
    }

    /**
     * Set up batch handler to consume events from the ring buffer. This handler will only process events after every IEventProcessor in this group has processed the event.
     * This method is generally used as part of a chain. For example if the handler A must process events before handler B: dw.handleEventsWith(A).then(B)
     *
     * \param handler
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> then(const std::shared_ptr<IEventHandler<T>> &handler) {
        return then(std::vector<std::shared_ptr<IEventHandler<T>>>{ handler });
    }

    /**
     * Set up custom event processors to handle events from the ring buffer. The Disruptor will automatically start these processors when Disruptor<T>::start() is called.
     * This method is generally used as part of a chain. For example if the handler A must process events before handler B: dw.handleEventsWith(A).then(B)
     *
     * \param eventProcessorFactories the event processor factories to use to create the event processors that will process events.
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> then(const std::vector<std::shared_ptr<IEventProcessorFactory<T>>> &eventProcessorFactories) {
        return handleEventsWith(eventProcessorFactories);
    }

    /**
     * Set up custom event processors to handle events from the ring buffer. The Disruptor will automatically start these processors when Disruptor<T>.start() is called.
     * This method is generally used as part of a chain. For example if the handler A must process events before handler B: dw.handleEventsWith(A).then(B)
     *
     * \param eventProcessorFactory the event processor factory to use to create the event processors that will process events.
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> then(const std::shared_ptr<IEventProcessorFactory<T>> &eventProcessorFactory) {
        return then(std::vector<std::shared_ptr<IEventProcessorFactory<T>>>{ eventProcessorFactory });
    }

    /**
     * Set up a worker pool to handle events from the ring buffer. The worker pool will only process events after every IEventProcessor in this group has processed the event.
     * Each event will be processed by one of the work handler instances. This method is generally used as part of a chain. For example if the handler A must process events
     * before the worker pool with handlers B, C: @code dw.handleEventsWith(A).thenHandleEventsWithWorkerPool(B, C);
     *
     * \param handlers the work handlers that will process events. Each work handler instance will provide an extra thread in the worker pool.
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> thenHandleEventsWithWorkerPool(const std::vector<std::shared_ptr<IWorkHandler<T>>> &handlers) {
        return handleEventsWithWorkerPool(handlers);
    }

    /**
     * Set up batch handlers to handle events from the ring buffer. These handlers will only process events after every IEventProcessor in this group has processed the event.
     *  This method is generally used as part of a chain. For example if A must process events before B: dw.after(A).handleEventsWith(B)
     *
     * \param handlers the batch handlers that will process events.
     * \returns EventHandlerGroupType that can be used to set up a event processor barrier over the created event processors.
     */
    std::shared_ptr<EventHandlerGroupType> handleEventsWith(const std::vector<std::shared_ptr<IEventHandler<T>>> &handlers) {
        return _disruptor->createEventProcessors(_sequences, handlers);
    }

    /**
     * Set up batch handlers to handle events from the ring buffer. These handlers will only process events after every IEventProcessor in this group has processed the event.
     * This method is generally used as part of a chain. For example if A must process events before B: dw.after(A).handleEventsWith(B)
     *
     * \param handler the batch handler that will process events.
     * \returns EventHandlerGroupType that can be used to set up a event processor barrier over the created event processors.
     */
    std::shared_ptr<EventHandlerGroupType> handleEventsWith(const std::shared_ptr<IEventHandler<T>> &handler) {
        return handleEventsWith(std::vector<std::shared_ptr<IEventHandler<T>>>{ handler });
    }

    /**
     * Set up custom event processors to handle events from the ring buffer. The Disruptor will automatically start these processors when Disruptor<T>::start() is called.
     * This method is generally used as part of a chain. For example if A must process events before B: dw.after(A).handleEventsWith(B)
     *
     * \param eventProcessorFactories the event processor factories to use to create the event processors that will process events.
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> handleEventsWith(const std::vector<std::shared_ptr<IEventProcessorFactory<T>>> &eventProcessorFactories) {
        return _disruptor->createEventProcessors(_sequences, eventProcessorFactories);
    }

    /**
     * Set up custom event processors to handle events from the ring buffer. The Disruptor will automatically start these processors when Disruptor<T>::start() is called.
     * This method is generally used as part of a chain. For example if A must process events before B: dw.after(A).handleEventsWith(B)
     *
     * \param eventProcessorFactory the event processor factory to use to create the event processors that will process events.
     * \returns EventHandlerGroupType that can be used to chain dependencies.
     */
    std::shared_ptr<EventHandlerGroupType> handleEventsWith(const std::shared_ptr<IEventProcessorFactory<T>> &eventProcessorFactory) {
        return handleEventsWith(std::vector<std::shared_ptr<IEventProcessorFactory<T>>>{ eventProcessorFactory });
    }

    /**
     * Set up a worker pool to handle events from the ring buffer. The worker pool will only process events after every IEventProcessor in this group has processed the event.
     * Each event will be processed by one of the work handler instances.
     * This method is generally used as part of a chain. For example if the handler A must process events before the worker pool with handlers B, C:
     * @code dw.handleEventsWith(A).thenHandleEventsWithWorkerPool(B, C);
     *
     * \param handlers the work handlers that will process events. Each work handler instance will provide an extra thread in the worker pool.
     * \returns a\returns <see cref="EventHandlerGroup{T}"/>\returns that can be used to set up a event processor barrier over the created event processors.
     */
    std::shared_ptr<EventHandlerGroupType> handleEventsWithWorkerPool(const std::vector<std::shared_ptr<IWorkHandler<T>>> &handlers) {
        return _disruptor->createWorkerPool(_sequences, handlers);
    }

    /**
     * Create a dependency barrier for the processors in this group. This allows custom event processors to have dependencies on BatchEventProcessor<T> created by the disruptor.
     *
     * \returns ISequenceBarrier including all the processors in this group.
     */
    std::shared_ptr<ISequenceBarrier> asSequenceBarrier() {
        return newBarrier(_disruptor->ringBuffer(), _sequences);
    }
};

} // namespace opencmw::disruptor
