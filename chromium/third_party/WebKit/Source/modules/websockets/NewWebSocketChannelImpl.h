/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
 */

#ifndef NewWebSocketChannelImpl_h
#define NewWebSocketChannelImpl_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/frame/ConsoleTypes.h"
#include "modules/websockets/WebSocketChannel.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebSocketHandle.h"
#include "public/platform/WebSocketHandleClient.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Deque.h"
#include "wtf/FastAllocBase.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class WebSocketHandshakeRequestInfo;
class WebSocketHandshakeResponseInfo;

} // namespace blink

namespace WebCore {

class Document;

// This class may replace MainThreadWebSocketChannel.
class NewWebSocketChannelImpl : public WebSocketChannel, public RefCounted<NewWebSocketChannelImpl>, public blink::WebSocketHandleClient, public ContextLifecycleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // You can specify the source file and the line number information
    // explicitly by passing the last parameter.
    // In the usual case, they are set automatically and you don't have to
    // pass it.
    static PassRefPtr<NewWebSocketChannelImpl> create(ExecutionContext* context, WebSocketChannelClient* client, const String& sourceURL = String(), unsigned lineNumber = 0)
    {
        return adoptRef(new NewWebSocketChannelImpl(context, client, sourceURL, lineNumber));
    }
    virtual ~NewWebSocketChannelImpl();

    // WebSocketChannel functions.
    virtual void connect(const KURL&, const String& protocol) OVERRIDE;
    virtual String subprotocol() OVERRIDE;
    virtual String extensions() OVERRIDE;
    virtual WebSocketChannel::SendResult send(const String& message) OVERRIDE;
    virtual WebSocketChannel::SendResult send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) OVERRIDE;
    virtual WebSocketChannel::SendResult send(PassRefPtr<BlobDataHandle>) OVERRIDE;
    virtual unsigned long bufferedAmount() const OVERRIDE;
    // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
    // argument to omit payload.
    virtual void close(int code, const String& reason) OVERRIDE;
    virtual void fail(const String& reason, MessageLevel, const String&, unsigned lineNumber) OVERRIDE;
    using WebSocketChannel::fail;
    virtual void disconnect() OVERRIDE;

    using RefCounted<NewWebSocketChannelImpl>::ref;
    using RefCounted<NewWebSocketChannelImpl>::deref;

    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;

private:
    enum MessageType {
        MessageTypeText,
        MessageTypeBlob,
        MessageTypeArrayBuffer,
    };

    struct Message {
        explicit Message(const String&);
        explicit Message(PassRefPtr<BlobDataHandle>);
        explicit Message(PassRefPtr<ArrayBuffer>);
        MessageType type;
        CString text;
        RefPtr<BlobDataHandle> blobDataHandle;
        RefPtr<ArrayBuffer> arrayBuffer;
    };

    struct ReceivedMessage {
        bool isMessageText;
        Vector<char> data;
    };

    class BlobLoader;

    NewWebSocketChannelImpl(ExecutionContext*, WebSocketChannelClient*, const String&, unsigned);
    void sendInternal();
    void flowControlIfNecessary();
    void failAsError(const String& reason) { fail(reason, ErrorMessageLevel, m_sourceURLAtConstruction, m_lineNumberAtConstruction); }
    void abortAsyncOperations();
    void handleDidClose(bool wasClean, unsigned short code, const String& reason);
    Document* document(); // can be called only when m_identifier > 0.

    // WebSocketHandleClient functions.
    virtual void didConnect(blink::WebSocketHandle*, bool fail, const blink::WebString& selectedProtocol, const blink::WebString& extensions) OVERRIDE;
    virtual void didStartOpeningHandshake(blink::WebSocketHandle*, const blink::WebSocketHandshakeRequestInfo&) OVERRIDE;
    virtual void didFinishOpeningHandshake(blink::WebSocketHandle*, const blink::WebSocketHandshakeResponseInfo&) OVERRIDE;
    virtual void didFail(blink::WebSocketHandle*, const blink::WebString& message) OVERRIDE;
    virtual void didReceiveData(blink::WebSocketHandle*, bool fin, blink::WebSocketHandle::MessageType, const char* data, size_t /* size */) OVERRIDE;
    virtual void didClose(blink::WebSocketHandle*, bool wasClean, unsigned short code, const blink::WebString& reason) OVERRIDE;
    virtual void didReceiveFlowControl(blink::WebSocketHandle*, int64_t quota) OVERRIDE;

    // Methods for BlobLoader.
    void didFinishLoadingBlob(PassRefPtr<ArrayBuffer>);
    void didFailLoadingBlob(FileError::ErrorCode);

    // WebSocketChannel functions.
    virtual void refWebSocketChannel() OVERRIDE { ref(); }
    virtual void derefWebSocketChannel() OVERRIDE { deref(); }

    // LifecycleObserver functions.
    // This object must be destroyed before the context.
    virtual void contextDestroyed() OVERRIDE { ASSERT_NOT_REACHED(); }

    // m_handle is a handle of the connection.
    // m_handle == 0 means this channel is closed.
    OwnPtr<blink::WebSocketHandle> m_handle;

    // m_client can be deleted while this channel is alive, but this class
    // expects that disconnect() is called before the deletion.
    WebSocketChannelClient* m_client;
    KURL m_url;
    // m_identifier > 0 means calling scriptContextExecution() returns a Document.
    unsigned long m_identifier;
    OwnPtr<BlobLoader> m_blobLoader;
    Deque<Message> m_messages;
    Vector<char> m_receivingMessageData;

    bool m_receivingMessageTypeIsText;
    int64_t m_sendingQuota;
    int64_t m_receivedDataSizeForFlowControl;
    unsigned long m_bufferedAmount;
    size_t m_sentSizeOfTopMessage;
    String m_subprotocol;
    String m_extensions;

    String m_sourceURLAtConstruction;
    unsigned m_lineNumberAtConstruction;

    static const int64_t receivedDataSizeForFlowControlHighWaterMark = 1 << 15;
};

} // namespace WebCore

#endif // NewWebSocketChannelImpl_h
