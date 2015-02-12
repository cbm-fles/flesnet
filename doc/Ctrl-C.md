# What happens when the user presses Ctrl-C?

The signal handler, installed in `main.cpp`, catches the signal and sets an atomic variable (`signal_status`) from initially zero to the value of the signal. A pointer to this variable has been passed to the `Application` object, which in turn has passed it on to the `ComputeBuffer` objects.

The `ComputeBuffer` object checks the value of `_signal_status*` in its main loop. If it is not zero (a signal has been received), it resets the variable to zero and calls its `request_abort()` method. Note: There is a potential race condition here, which may cause more than one `ComputeBuffer` object to receive the signal.

The `ComputeBuffer::request_abort()` method in turn calls in a loop the `ComputeNodeConnection::request_abort()` methods of all associated connections.

The `ComputeNodeConnection::request_abort()` method only sets the value of the connection's `_send_status_message.request_abort` flag to true. This flag is part of the `ComputeNodeStatusMessage` structure, which is peridodically transmitted to the connection's input node at the other end.

The next time `ComputeNodeConnection::on_complete_recv()` is called (after having received a pointer update from the corresponting input node), `ComputeNodeConnection::post_send_status_message()` is called, which sends the `ComputeNodeStatusMessage` structure with the `request_abort` flag set to the input node.

At the other end, `InputChannelSender::on_completion()` is called upon recept of the pointer update. It checks the flag via the `InputChannelConnection::request_abort_flag()` getter method, which returns the value of the flag. If the flag is set for the received update, the `InputChannelSender::_abort` flag is set to `true`.

The value of this `_abort` flag is checked in the `InputChannelSender` main loop, where it is an ending condition for the sending loop just like reaching `_max_timeslice_number`. After the sending loop has ended, the `InputChannelConnection::finalize()` methods of all connections are called with the value of the `_abort` flag as a parameter indicating which of the two ending conditions has triggered.

In `InputChannelConnection::finalize()`, the connection's `_finalize` flag is set to `true`, and `_abort` stores the value of the `abort` parameter. If possible (it is `_our_turn`), the status message update is immediately sent to the corresponding compute node. If the compute node buffer is already empty (`_cn_wp == _cn_ack`) or we are aborting, the message is sent with the `InputChannelStatusMessage::final` and `InputChannelStatusMessage::abort` flags set correspondingly, otherwise (finalize without abort and still data in compute node buffer) an ordinary pointer update is sent.

If it has not been `_our_turn`, then the status message is sent the next time the `InputChannelConnection::on_complete_recv()` method is called. Here the logic is: If we are finalizing and no new data has been sent since last pointer update, we send a dummy update to allow the compute node to report back when it has consumed the data and its buffer is empty. If the compute node buffer is already empty or we are aborting, we send the update with the `.final` flag set.

... to be continued ...

Questions:

- what if `_cn_wp != _send_status_message.wp` in `on_complete_recv()`?
- `sync_buffer_positions()` is called ty timer even after `finalize()` - problem? required?

