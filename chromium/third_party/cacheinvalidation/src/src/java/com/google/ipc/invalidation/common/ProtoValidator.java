/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.ipc.invalidation.common;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.common.ProtoValidator.FieldInfo.Presence;
import com.google.ipc.invalidation.util.BaseLogger;
import com.google.ipc.invalidation.util.TypedUtil;
import com.google.protobuf.MessageLite;

import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.Set;


/**
 * Base class for writing protocol buffer validators. This is used in conjunction with
 * {@code ProtoAccessorGenerator}.
 *
 */
public abstract class ProtoValidator {
  /** Class providing access to protocol buffer fields in a generic way. */
  public interface Accessor {
    public boolean hasField(MessageLite message, Descriptor field);
    public Object getField(MessageLite message, Descriptor field);
    public Collection<String> getAllFieldNames();
  }

  /** Class naming a protocol buffer field in a generic way. */
  public static class Descriptor {
    private final String name;

    public Descriptor(String name) {
      this.name = name;
    }
    /** Returns the name of the described field. */
    public String getName() {
      return name;
    }
    @Override
    public String toString() {
      return "Descriptor for field " + name;
    }
  }

  /** Describes how to validate a message. */
  public static class MessageInfo {
    /** Protocol buffer descriptor for the message. */
    private final Accessor messageAccessor;

    /** Information about required and optional fields in this message. */
    private final Set<FieldInfo> fieldInfo = new HashSet<FieldInfo>();

    private int numRequiredFields;

    /**
     * Constructs a message info.
     *
     * @param messageAccessor descriptor for the protocol buffer
     * @param fields information about the fields
     */
    public MessageInfo(Accessor messageAccessor, FieldInfo... fields) {
      // Track which fields in the message descriptor have not yet been covered by a FieldInfo.
      // We'll use this to verify that we get a FieldInfo for every field.
      Set<String> unusedDescriptors = new HashSet<String>();
      unusedDescriptors.addAll(messageAccessor.getAllFieldNames());

      this.messageAccessor = messageAccessor;
      for (FieldInfo info : fields) {
        // Lookup the field given the name in the FieldInfo.
        boolean removed = TypedUtil.remove(unusedDescriptors, info.getFieldDescriptor().getName());
        Preconditions.checkState(removed, "Bad field: %s", info.getFieldDescriptor().getName());

        // Add the field info to the number -> info map.
        fieldInfo.add(info);

        if (info.getPresence() == Presence.REQUIRED) {
          ++numRequiredFields;
        }
      }
      Preconditions.checkState(unusedDescriptors.isEmpty(), "Not all fields specified in %s: %s",
          messageAccessor, unusedDescriptors);
    }

    /** Returns the stored field information. */
    protected Collection<FieldInfo> getAllFields() {
      return fieldInfo;
    }

    /**
     * Function called after the presence/absence of all fields in this message and its child
     * messages have been verified. Should be overridden to enforce additional semantic constraints
     * beyond field presence/absence if needed.
     */
    protected boolean postValidate(MessageLite message) {
      return true;
    }

    /** Returns the number of required fields for messages of this type. */
    public int getNumRequiredFields() {
      return numRequiredFields;
    }
  }

  /** Describes a field in a message. */
  protected static class FieldInfo {
    /**
     * Whether the field is required or optional. A repeated field where at least one value
     * must be set should use {@code REQUIRED}.
     */
    enum Presence {
      REQUIRED,
      OPTIONAL
    }

    /** Name of the field in the containing message. */
    private final Descriptor fieldDescriptor;

    /** Whether the field is required or optional. */
    private final Presence presence;

    /** If not {@code null}, message info describing how to validate the field. */
    private final MessageInfo messageInfo;

    /**
     * Constructs an instance.
     *
     * @param fieldDescriptor identifier for the field
     * @param presence required/optional
     * @param messageInfo if not {@code null}, describes how to validate the field
     */
    FieldInfo(Descriptor fieldDescriptor, Presence presence,
        MessageInfo messageInfo) {
      this.fieldDescriptor = fieldDescriptor;
      this.presence = presence;
      this.messageInfo = messageInfo;
    }

    /** Returns the name of the field. */
    public Descriptor getFieldDescriptor() {
      return fieldDescriptor;
    }

    /** Returns the presence information for the field. */
    Presence getPresence() {
      return presence;
    }

    /** Returns the validation information for the field. */
    MessageInfo getMessageInfo() {
      return messageInfo;
    }

    /** Returns whether the field needs additional validation. */
    boolean requiresAdditionalValidation() {
      return messageInfo != null;
    }

    /**
     * Returns a new instance describing a required field with name {@code fieldName} and validation
     * specified by {@code messageInfo}.
     */
    public static FieldInfo newRequired(Descriptor fieldDescriptor, MessageInfo messageInfo) {
      return new FieldInfo(fieldDescriptor, Presence.REQUIRED,
          Preconditions.checkNotNull(messageInfo, "messageInfo cannot be null"));
    }

    /**
     * Returns a new instance describing a required field with name {@code fieldName} and no
     * additional validation.
     */
    public static FieldInfo newRequired(Descriptor fieldDescriptor) {
      return new FieldInfo(fieldDescriptor, Presence.REQUIRED, null);
    }

    /**
     * Returns a new instance describing an optional field with name {@code fieldName} and
     * validation specified by {@code messageInfo}.
     */
    public static FieldInfo newOptional(Descriptor fieldDescriptor, MessageInfo messageInfo) {
      return new FieldInfo(fieldDescriptor, Presence.OPTIONAL,
          Preconditions.checkNotNull(messageInfo));
    }

    /**
     * Returns a new instance describing an optional field with name {@code fieldName} and no
     * additional validation.
     */
    public static FieldInfo newOptional(Descriptor fieldDescriptor) {
      return new FieldInfo(fieldDescriptor, Presence.OPTIONAL, null);
    }
  }

  /** Logger for errors */
  protected final BaseLogger logger;

  protected ProtoValidator(BaseLogger logger) {
    this.logger = logger;
  }

  /**
   * Returns an {@link Iterable} over the instance(s) of {@code field} in {@code message}. This
   * provides a uniform way to handle both singleton and repeated fields in protocol buffers, which
   * are accessed using different calls in the protocol buffer API.
   */
  @SuppressWarnings("unchecked")
  
  protected static <FieldType> Iterable<FieldType> getFieldIterable(final MessageLite message,
      final Accessor messageAccessor, final Descriptor fieldDescriptor) {
    final Object obj = messageAccessor.getField(message, fieldDescriptor);
    if (obj instanceof List) {
      return (List<FieldType>) obj;
    } else {
      // Otherwise, just use a singleton iterator.
      return new Iterable<FieldType>() {
        @Override
        public Iterator<FieldType> iterator() {
          return new Iterator<FieldType>() {
            boolean done;
            @Override
            public boolean hasNext() {
              return !done;
            }

            @Override
            public FieldType next() {
              if (done) {
                throw new NoSuchElementException();
              }
              done = true;
              return (FieldType) obj;
            }

            @Override
            public void remove() {
              throw new UnsupportedOperationException("Not allowed");
            }
          };
        }
      };
    }
  }

  /**
   * Returns whether {@code message} is valid.
   * @param messageInfo specification of validity for {@code message}
   */
  
  protected boolean checkMessage(MessageLite message, MessageInfo messageInfo) {
    for (FieldInfo fieldInfo : messageInfo.getAllFields()) {
      Descriptor fieldDescriptor = fieldInfo.getFieldDescriptor();
      boolean isFieldPresent =
          messageInfo.messageAccessor.hasField(message, fieldDescriptor);

      // If the field must be present but isn't, fail.
      if ((fieldInfo.getPresence() == FieldInfo.Presence.REQUIRED) && !(isFieldPresent)) {
        logger.warning("Required field not set: %s", fieldInfo.getFieldDescriptor().getName());
        return false;
      }

      // If the field is present and requires its own validation, validate it.
      if (isFieldPresent && fieldInfo.requiresAdditionalValidation()) {
        for (MessageLite subMessage : TiclMessageValidator2.<MessageLite>getFieldIterable(
            message, messageInfo.messageAccessor, fieldDescriptor)) {
          if (!checkMessage(subMessage, fieldInfo.getMessageInfo())) {
            return false;
          }
        }
      }
    }

    // Once we've validated all fields, post-validate this message.
    if (!messageInfo.postValidate(message)) {
      logger.info("Failed post-validation of message (%s): %s",
          message.getClass().getSimpleName(), message);
      return false;
    }
    return true;
  }
}
