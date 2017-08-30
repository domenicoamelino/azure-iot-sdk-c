// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include <stdlib.h>
#include "uamqp_messaging.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/amqpvalue.h"
#include "iothub_message.h"
#ifndef RESULT_OK
#define RESULT_OK 0
#endif

static int encode_callback(void* context, const unsigned char* bytes, size_t length)
{
    BINARY_DATA* message_body_binary = (BINARY_DATA*)context;
    (void)memcpy((unsigned char*)message_body_binary->bytes + message_body_binary->length, bytes, length);
    message_body_binary->length += length;
    return 0;
}

// Codes_SRS_UAMQP_MESSAGING_31_112: [If optional message-id is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
static int set_message_id_if_needed(IOTHUB_MESSAGE_HANDLE messageHandle, PROPERTIES_HANDLE uamqp_message_properties)
{
    int result;
    const char* messageId;

    if (NULL != (messageId = IoTHubMessage_GetMessageId(messageHandle)))
    {
        AMQP_VALUE uamqp_message_id;

        if (NULL == (uamqp_message_id = amqpvalue_create_string(messageId)))
        {
            LogError("Failed amqpvalue_create_string for message_id");
            result = __FAILURE__;
        }
        else if (0 != (result = properties_set_message_id(uamqp_message_properties, uamqp_message_id)))
        {
            LogError("Failed properties_set_message_id, result = %d", result);
            result = __FAILURE__;
        }
        else
        {
            result = RESULT_OK;
        }

        if (NULL != uamqp_message_id)
        {
            amqpvalue_destroy(uamqp_message_id);
        }
    }
    else
    {
        result = RESULT_OK;
    }

    return result;
}

// Codes_SRS_UAMQP_MESSAGING_31_113: [If optional correlation-id is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
static int set_message_correlation_id_if_needed(IOTHUB_MESSAGE_HANDLE messageHandle, PROPERTIES_HANDLE uamqp_message_properties)
{
    int result;
    const char* correlationId;

    if (NULL != (correlationId = IoTHubMessage_GetCorrelationId(messageHandle)))
    {
        AMQP_VALUE uamqp_correlation_id;

        if (NULL == (uamqp_correlation_id = amqpvalue_create_string(correlationId)))
        {
            LogError("Failed amqpvalue_create_string for message_id");
            result = __FAILURE__;
        }
        else if (0 != (result = properties_set_correlation_id(uamqp_message_properties, uamqp_correlation_id)))
        {
            LogError("Failed properties_set_correlation_id, result = %d", result);
            result = __FAILURE__;
        }
        else
        {
            result = RESULT_OK;
        }

        if (NULL != uamqp_correlation_id)
        {
            amqpvalue_destroy(uamqp_correlation_id);
        }
    }
    else
    {
        result = RESULT_OK;
    }

    return result;
}

// Codes_SRS_UAMQP_MESSAGING_31_114: [If optional content-type is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
static int set_message_content_type_if_needed(IOTHUB_MESSAGE_HANDLE messageHandle, PROPERTIES_HANDLE uamqp_message_properties)
{
    int result;
    const char* content_type;

    if (NULL != (content_type = IoTHubMessage_GetContentTypeSystemProperty(messageHandle)))
    {
        if (0 != (result = properties_set_content_type(uamqp_message_properties, content_type)))
        {
            LogError("Failed properties_set_content_type, result = %d", result);
            result = __FAILURE__;
        }
    }
    else
    {
        result = RESULT_OK;
    }

    return result;
}

// Codes_SRS_UAMQP_MESSAGING_31_115: [If optional content-encoding is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
static int set_message_content_encoding_if_needed(IOTHUB_MESSAGE_HANDLE messageHandle, PROPERTIES_HANDLE uamqp_message_properties)
{
    int result;
    const char* content_encoding;

    if (NULL != (content_encoding = IoTHubMessage_GetContentEncodingSystemProperty(messageHandle)))
    {
        if (0 != (result = properties_set_content_encoding(uamqp_message_properties, content_encoding)))
        {
            LogError("Failed properties_set_content_encoding, result = %d", result);
            result = __FAILURE__;
        }
    }
    else
    {
        result = RESULT_OK;
    }

    return result;
}


// Codes_SRS_UAMQP_MESSAGING_31_116: [Gets message properties associated with the IOTHUB_MESSAGE_HANDLE to encode, returning the properties and their encoded length.  Errors stop processing on this message.]
static int create_message_properties_to_encode(IOTHUB_MESSAGE_HANDLE messageHandle, AMQP_VALUE *message_properties, size_t *message_properties_length)
{
    PROPERTIES_HANDLE uamqp_message_properties = NULL;
    int result;

    if (NULL == (uamqp_message_properties = properties_create()))
    {
        LogError("Failed on properties_create()");
        result = __FAILURE__;
    }
    else if (RESULT_OK != set_message_id_if_needed(messageHandle, uamqp_message_properties))
    {
        LogError("Failed on set_message_id_if_needed()");
        result = __FAILURE__;
    }
    else if (RESULT_OK != set_message_correlation_id_if_needed(messageHandle, uamqp_message_properties))
    {
        LogError("Failed on set_message_correlation_id_if_needed()");
        result = __FAILURE__;
    }
    else if (RESULT_OK != set_message_content_type_if_needed(messageHandle, uamqp_message_properties))
    {
        LogError("Failed on set_message_content_type_if_needed()");
        result = __FAILURE__;
    }
    else if (RESULT_OK != set_message_content_encoding_if_needed(messageHandle, uamqp_message_properties))
    {
        LogError("Failed on set_message_content_encoding_if_needed()");
        result = __FAILURE__;
    }
    else if (NULL == (*message_properties = amqpvalue_create_properties(uamqp_message_properties)))
    {
        LogError("Failed on amqpvalue_create_properties()");
        result = __FAILURE__;
    }
    else if (0 != (amqpvalue_get_encoded_size(*message_properties, message_properties_length)))
    {
        LogError("Failed on amqpvalue_get_encoded_size()");
        result = __FAILURE__;
    }
    else
    {
        result = RESULT_OK;
    }

    return result;
}

// Codes_SRS_UAMQP_MESSAGING_31_117: [Get application message properties associated with the IOTHUB_MESSAGE_HANDLE to encode, returning the properties and their encoded length.  Errors stop processing on this message.]
static int create_application_properties_to_encode(IOTHUB_MESSAGE_HANDLE messageHandle, AMQP_VALUE *application_properties, size_t *application_properties_length)
{
    MAP_HANDLE properties_map;
    const char* const* property_keys;
    const char* const* property_values;
    size_t property_count = 0;
    AMQP_VALUE uamqp_properties_map;
    int result;

    if (NULL == (properties_map = IoTHubMessage_Properties(messageHandle)))
    {
        LogError("Failed to get property map from IoTHub message.");
        result = __FAILURE__;
    }
    else if (0 != (result = Map_GetInternals(properties_map, &property_keys, &property_values, &property_count)))
    {
        LogError("Failed reading the incoming uAMQP message properties (return code %d).", result);
        result = __FAILURE__;
    }
    else if (property_count > 0)
    {
        size_t i;
        if (NULL == (uamqp_properties_map = amqpvalue_create_map()))
        {
            LogError("amqpvalue_create_map failed");
            result = __FAILURE__;
        }
        else
        {
            for (i = 0; i < property_count; i++)
            {
                AMQP_VALUE map_property_key;
                AMQP_VALUE map_property_value;

                if ((map_property_key = amqpvalue_create_string(property_keys[i])) == NULL)
                {
                    LogError("Failed amqpvalue_create_string for key");
                    result = __FAILURE__;
                    break;
                }

                if ((map_property_value = amqpvalue_create_string(property_values[i])) == NULL)
                {
                    LogError("Failed amqpvalue_create_string for value");
                    amqpvalue_destroy(map_property_key);
                    result = __FAILURE__;
                    break;
                }

                if (0 != (result = amqpvalue_set_map_value(uamqp_properties_map, map_property_key, map_property_value)))
                {
                    LogError("Failed amqpvalue_set_map_value, result = %d", result);
                    amqpvalue_destroy(map_property_key);
                    amqpvalue_destroy(map_property_value);
                    result = __FAILURE__;
                    break;
                }

                amqpvalue_destroy(map_property_key);
                amqpvalue_destroy(map_property_value);
            }

            if (0 == result)
            {
                if (NULL == (*application_properties = amqpvalue_create_application_properties(uamqp_properties_map)))
                {
                    LogError("Failed amqpvalue_create_application_properties");
                    result = __FAILURE__;
                }
                else if (0 != (result = amqpvalue_get_encoded_size(*application_properties, application_properties_length)))
                {
                    LogError("Failed amqpvalue_get_encoded_size, result = %d", result);
                    result = __FAILURE__;
                }
            }
        }
    }

    return result;
}


// Codes_SRS_UAMQP_MESSAGING_31_118: [Gets data associated with IOTHUB_MESSAGE_HANDLE to encode, either from underlying byte array or string format.  Errors stop processing on this message.]
static int create_data_to_encode(IOTHUB_MESSAGE_HANDLE messageHandle, AMQP_VALUE *data_value, size_t *data_length)
{
    int result;

    IOTHUBMESSAGE_CONTENT_TYPE contentType = IoTHubMessage_GetContentType(messageHandle);
    const char* messageContent = NULL;
    size_t messageContentSize = 0;

    if ((contentType == IOTHUBMESSAGE_BYTEARRAY) &&
        IoTHubMessage_GetByteArray(messageHandle, (const unsigned char **)&messageContent, &messageContentSize) != IOTHUB_MESSAGE_OK)
    {
        LogError("Failed getting the BYTE array representation of the IOTHUB_MESSAGE_HANDLE instance.");
        result = __FAILURE__;
    }
    else if ((contentType == IOTHUBMESSAGE_STRING) &&
        ((messageContent = IoTHubMessage_GetString(messageHandle)) == NULL))
    {
        LogError("Failed getting the STRING representation of the IOTHUB_MESSAGE_HANDLE instance.");
        result = __FAILURE__;
    }
    else if (contentType == IOTHUBMESSAGE_UNKNOWN)
    {
        LogError("Cannot parse IOTHUB_MESSAGE_HANDLE with content type IOTHUBMESSAGE_UNKNOWN.");
        result = __FAILURE__;
    }
    else
    {
        if (contentType == IOTHUBMESSAGE_STRING)
        {
            messageContentSize = strlen(messageContent);
        }
    
        data bin_data;
        bin_data.bytes = (const unsigned char *)messageContent;
        bin_data.length = (uint32_t)messageContentSize;

        if (NULL == (*data_value = amqpvalue_create_data(bin_data)))
        {
            LogError("amqpvalue_create_data failed");
            result = __FAILURE__;
        }
        else if (0 != amqpvalue_get_encoded_size(*data_value, data_length))
        {
            LogError("amqpvalue_get_encoded_size failed");
            result = __FAILURE__;
        }
        else
        {
            result = RESULT_OK;
        }
    }

    return result;
}

// Codes_SRS_UAMQP_MESSAGING_31_120: [Create a blob that contains AMQP encoding of IOTHUB_MESSAGE_HANDLE.  Errors stop processing on this message.]
int create_amqp_message_data(IOTHUB_MESSAGE_HANDLE message_handle, BINARY_DATA* body_binary_data)
{
    int result;

    AMQP_VALUE message_properties = NULL;
    AMQP_VALUE application_properties = NULL;
    AMQP_VALUE data_value = NULL;
    size_t message_properties_length = 0;
    size_t application_properties_length = 0;
    size_t data_length = 0;

    body_binary_data->bytes = NULL;
    body_binary_data->length = 0;

    if (RESULT_OK != (result = create_message_properties_to_encode(message_handle, &message_properties, &message_properties_length)))
    {
        LogError("create_message_properties_to_encode() failed, result = %d", result);
    }
    else if (RESULT_OK != (result = create_application_properties_to_encode(message_handle, &application_properties, &application_properties_length)))
    {
        LogError("create_application_properties_to_encode() failed, result = %d", result);
    }
    else if (RESULT_OK != (result = create_data_to_encode(message_handle, &data_value, &data_length)))
    {
        LogError("create_data_to_encode() failed, result = %d", result);
    }
    else if (NULL == (body_binary_data->bytes = malloc(message_properties_length + application_properties_length + data_length)))
    {
        LogError("malloc of %d bytes failed", message_properties_length + application_properties_length + data_length);
        result = __FAILURE__;
    }
    // Codes_SRS_UAMQP_MESSAGING_31_119: [Invoke underlying AMQP encode routines on data waiting to be encoded.  Errors stop processing on this message.]
    else if (RESULT_OK != (result = amqpvalue_encode(message_properties, &encode_callback, body_binary_data)))
    {
        LogError("amqpvalue_encode() for message properties failed, result = %d", result);
    }
    else if ((application_properties_length > 0) && (RESULT_OK != (result = amqpvalue_encode(application_properties, &encode_callback, body_binary_data))))
    {
        LogError("amqpvalue_encode() for application properties failed, result = %d", result);
    }
    else if (RESULT_OK != (result = amqpvalue_encode(data_value, &encode_callback, body_binary_data)))
    {
        LogError("amqpvalue_encode() for data value failed, result = %d", result);
    }
    else
    {
        body_binary_data->length = message_properties_length + application_properties_length + data_length;
        result = RESULT_OK;
    }

    return result;
}

static int readPropertiesFromuAMQPMessage(IOTHUB_MESSAGE_HANDLE iothub_message_handle, MESSAGE_HANDLE uamqp_message)
{
    int return_value;
    PROPERTIES_HANDLE uamqp_message_properties;
    AMQP_VALUE uamqp_message_property;
    const char* uamqp_message_property_value;
    int api_call_result;

    // Codes_SRS_UAMQP_MESSAGING_09_008: [The uAMQP message properties shall be retrieved using message_get_properties().]
    if ((api_call_result = message_get_properties(uamqp_message, &uamqp_message_properties)) != 0)
    {
        // Codes_SRS_UAMQP_MESSAGING_09_009: [If message_get_properties() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
        LogError("Failed to get property properties map from uAMQP message (error code %d).", api_call_result);
        return_value = __FAILURE__;
    }
    else
    {
        return_value = 0; // Properties 'message-id' and 'correlation-id' are optional according to the AMQP 1.0 spec.

        // Codes_SRS_UAMQP_MESSAGING_09_010: [The message-id property shall be read from the uAMQP message by calling properties_get_message_id.]
        if ((api_call_result = properties_get_message_id(uamqp_message_properties, &uamqp_message_property)) != 0)
        {
            // Codes_SRS_UAMQP_MESSAGING_09_011: [If properties_get_message_id fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
            LogInfo("Failed to get value of uAMQP message 'message-id' property (%d). No failure, since it is optional.", api_call_result);
        }
        // Codes_SRS_UAMQP_MESSAGING_09_012: [The type of the message-id property value shall be obtained using amqpvalue_get_type().]
        // Codes_SRS_UAMQP_MESSAGING_09_013: [If the type of the message-id property value is AMQP_TYPE_NULL, IoTHubMessage_CreateFromuAMQPMessage() shall skip processing the message-id (as it is optional) and continue normally.]
        else if (amqpvalue_get_type(uamqp_message_property) != AMQP_TYPE_NULL)
        {
            // Codes_SRS_UAMQP_MESSAGING_09_014: [The message-id value shall be retrieved from the AMQP_VALUE as char* by calling amqpvalue_get_string().]
            if ((api_call_result = amqpvalue_get_string(uamqp_message_property, &uamqp_message_property_value)) != 0)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_015: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                LogError("Failed to get value of uAMQP message 'message-id' property (%d).", api_call_result);
                return_value = __FAILURE__;
            }
            // Codes_SRS_UAMQP_MESSAGING_09_016: [The message-id property shall be set on the IOTHUB_MESSAGE_HANDLE instance by calling IoTHubMessage_SetMessageId(), passing the value read from the uAMQP message.]
            else if (IoTHubMessage_SetMessageId(iothub_message_handle, uamqp_message_property_value) != IOTHUB_MESSAGE_OK)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_017: [If IoTHubMessage_SetMessageId fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                LogError("Failed to set IOTHUB_MESSAGE_HANDLE 'message-id' property.");
                return_value = __FAILURE__;
            }
        }

        // Codes_SRS_UAMQP_MESSAGING_09_018: [The correlation-id property shall be read from the uAMQP message by calling properties_get_correlation_id.]
        if ((api_call_result = properties_get_correlation_id(uamqp_message_properties, &uamqp_message_property)) != 0)
        {
            // Codes_SRS_UAMQP_MESSAGING_09_019: [If properties_get_correlation_id() fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
            LogError("Failed to get value of uAMQP message 'correlation-id' property (%d). No failure, since it is optional.", api_call_result);
        }
        // Codes_SRS_UAMQP_MESSAGING_09_020: [The type of the correlation-id property value shall be obtained using amqpvalue_get_type().]
        // Codes_SRS_UAMQP_MESSAGING_09_021: [If the type of the correlation-id property value is AMQP_TYPE_NULL, IoTHubMessage_CreateFromuAMQPMessage() shall skip processing the correlation-id (as it is optional) and continue normally.]
        else if (amqpvalue_get_type(uamqp_message_property) != AMQP_TYPE_NULL)
        {
            // Codes_SRS_UAMQP_MESSAGING_09_022: [The correlation-id value shall be retrieved from the AMQP_VALUE as char* by calling amqpvalue_get_string.]
            if ((api_call_result = amqpvalue_get_string(uamqp_message_property, &uamqp_message_property_value)) != 0)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_023: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                LogError("Failed to get value of uAMQP message 'correlation-id' property (%d).", api_call_result);
                return_value = __FAILURE__;
            }
            // Codes_SRS_UAMQP_MESSAGING_09_024: [The correlation-id property shall be set on the IOTHUB_MESSAGE_HANDLE by calling IoTHubMessage_SetCorrelationId, passing the value read from the uAMQP message.]
            else if (IoTHubMessage_SetCorrelationId(iothub_message_handle, uamqp_message_property_value) != IOTHUB_MESSAGE_OK)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_025: [If IoTHubMessage_SetCorrelationId fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                LogError("Failed to set IOTHUB_MESSAGE_HANDLE 'correlation-id' property.");
                return_value = __FAILURE__;
            }
        }
        // Codes_SRS_UAMQP_MESSAGING_09_026: [IoTHubMessage_CreateFromuAMQPMessage() shall destroy the uAMQP message properties (obtained with message_get_properties()) by calling properties_destroy().]
        properties_destroy(uamqp_message_properties);
    }

    return return_value;
}

static int readApplicationPropertiesFromuAMQPMessage(IOTHUB_MESSAGE_HANDLE iothub_message_handle, MESSAGE_HANDLE uamqp_message)
{
    int result;
    AMQP_VALUE uamqp_app_properties = NULL;
    AMQP_VALUE uamqp_app_properties_ipdv = NULL;
    uint32_t property_count = 0;
    MAP_HANDLE iothub_message_properties_map;

    // Codes_SRS_UAMQP_MESSAGING_09_027: [The IOTHUB_MESSAGE_HANDLE properties shall be retrieved using IoTHubMessage_Properties.]
    if ((iothub_message_properties_map = IoTHubMessage_Properties(iothub_message_handle)) == NULL)
    {
        // Codes_SRS_UAMQP_MESSAGING_09_028: [If IoTHubMessage_Properties fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
        LogError("Failed to get property map from IoTHub message.");
        result = __FAILURE__;
    }
    // Codes_SRS_UAMQP_MESSAGING_09_029: [The uAMQP message application properties shall be retrieved using message_get_application_properties.]
    else if ((result = message_get_application_properties(uamqp_message, &uamqp_app_properties)) != 0)
    {
        // Codes_SRS_UAMQP_MESSAGING_09_030: [If message_get_application_properties fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
        LogError("Failed reading the incoming uAMQP message properties (return code %d).", result);
        result = __FAILURE__;
    }
    else
    {
        // Codes_SRS_UAMQP_MESSAGING_09_031: [If message_get_application_properties succeeds but returns a NULL application properties map (there are no properties), IoTHubMessage_CreateFromuAMQPMessage() shall skip processing the properties and continue normally.]
        if (uamqp_app_properties == NULL)
        {
            result = 0;
        }
        else
        {
            // Codes_SRS_UAMQP_MESSAGING_09_032: [The actual uAMQP message application properties should be extracted from the result of message_get_application_properties using amqpvalue_get_inplace_described_value.]
            if ((uamqp_app_properties_ipdv = amqpvalue_get_inplace_described_value(uamqp_app_properties)) == NULL)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_033: [If amqpvalue_get_inplace_described_value fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                LogError("Failed getting the map of uAMQP message application properties (return code %d).", result);
                result = __FAILURE__;
            }
            // Codes_SRS_UAMQP_MESSAGING_09_034: [The number of items in the uAMQP message application properties shall be obtained using amqpvalue_get_map_pair_count.]
            else if ((result = amqpvalue_get_map_pair_count(uamqp_app_properties_ipdv, &property_count)) != 0)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_035: [If amqpvalue_get_map_pair_count fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                LogError("Failed reading the number of values in the uAMQP property map (return code %d).", result);
                result = __FAILURE__;
            }
            else
            {
                // Codes_SRS_UAMQP_MESSAGING_09_036: [IoTHubMessage_CreateFromuAMQPMessage() shall iterate through each uAMQP application property and add it to IOTHUB_MESSAGE_HANDLE properties.]
                uint32_t i;
                for (i = 0; result == RESULT_OK && i < property_count; i++)
                {
                    AMQP_VALUE map_key_name = NULL;
                    AMQP_VALUE map_key_value = NULL;
                    const char *key_name;
                    const char* key_value;

                    // Codes_SRS_UAMQP_MESSAGING_09_037: [The uAMQP application property name and value shall be obtained using amqpvalue_get_map_key_value_pair.]
                    if ((result = amqpvalue_get_map_key_value_pair(uamqp_app_properties_ipdv, i, &map_key_name, &map_key_value)) != 0)
                    {
                        // Codes_SRS_UAMQP_MESSAGING_09_038: [If amqpvalue_get_map_key_value_pair fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                        LogError("Failed reading the key/value pair from the uAMQP property map (return code %d).", result);
                        result = __FAILURE__;
                    }

                    // Codes_SRS_UAMQP_MESSAGING_09_039: [The uAMQP application property name shall be extracted as string using amqpvalue_get_string.]
                    else if ((result = amqpvalue_get_string(map_key_name, &key_name)) != 0)
                    {
                        // Codes_SRS_UAMQP_MESSAGING_09_040: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                        LogError("Failed parsing the uAMQP property name (return code %d).", result);
                        result = __FAILURE__;
                    }
                    // Codes_SRS_UAMQP_MESSAGING_09_041: [The uAMQP application property value shall be extracted as string using amqpvalue_get_string.]
                    else if ((result = amqpvalue_get_string(map_key_value, &key_value)) != 0)
                    {
                        // Codes_SRS_UAMQP_MESSAGING_09_042: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                        LogError("Failed parsing the uAMQP property value (return code %d).", result);
                        result = __FAILURE__;
                    }
                    // Codes_SRS_UAMQP_MESSAGING_09_043: [The application property name and value shall be added to IOTHUB_MESSAGE_HANDLE properties using Map_AddOrUpdate.]
                    else if (Map_AddOrUpdate(iothub_message_properties_map, key_name, key_value) != MAP_OK)
                    {
                        // Codes_SRS_UAMQP_MESSAGING_09_044: [If Map_AddOrUpdate fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
                        LogError("Failed to add/update IoTHub message property map.");
                        result = __FAILURE__;
                    }

                    // Codes_SRS_UAMQP_MESSAGING_09_045: [IoTHubMessage_CreateFromuAMQPMessage() shall destroy the uAMQP message property name and value (obtained with amqpvalue_get_string) by calling amqpvalue_destroy().]
                    if (map_key_name != NULL)
                    {
                        amqpvalue_destroy(map_key_name);
                    }

                    if (map_key_value != NULL)
                    {
                        amqpvalue_destroy(map_key_value);
                    }
                }
            }

            // Codes_SRS_UAMQP_MESSAGING_09_046: [IoTHubMessage_CreateFromuAMQPMessage() shall destroy the uAMQP message property (obtained with message_get_application_properties) by calling amqpvalue_destroy().]
            amqpvalue_destroy(uamqp_app_properties);
        }
    }

    return result;
}

int IoTHubMessage_CreateFromUamqpMessage(MESSAGE_HANDLE uamqp_message, IOTHUB_MESSAGE_HANDLE* iothubclient_message)
{
    int result = __FAILURE__;

    IOTHUB_MESSAGE_HANDLE iothub_message = NULL;
    MESSAGE_BODY_TYPE body_type;

    // Codes_SRS_UAMQP_MESSAGING_09_001: [The body type of the uAMQP message shall be retrieved using message_get_body_type().]
    if (message_get_body_type(uamqp_message, &body_type) != 0)
    {
        // Codes_SRS_UAMQP_MESSAGING_09_002: [If message_get_body_type() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
        LogError("Failed to get the type of the uamqp message.");
        result = __FAILURE__;
    }
    else
    {
        // Codes_SRS_UAMQP_MESSAGING_09_003: [If the uAMQP message body type is MESSAGE_BODY_TYPE_DATA, the body data shall be treated as binary data.]
        if (body_type == MESSAGE_BODY_TYPE_DATA)
        {
            // Codes_SRS_UAMQP_MESSAGING_09_004: [The uAMQP message body data shall be retrieved using message_get_body_amqp_data().]
            BINARY_DATA binary_data;
            if (message_get_body_amqp_data_in_place(uamqp_message, 0, &binary_data) != 0)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_005: [If message_get_body_amqp_data() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
                LogError("Failed to get the body of the uamqp message.");
                result = __FAILURE__;
            }
            // Codes_SRS_UAMQP_MESSAGING_09_006: [The IOTHUB_MESSAGE instance shall be created using IoTHubMessage_CreateFromByteArray(), passing the uAMQP body bytes as parameter.]
            else if ((iothub_message = IoTHubMessage_CreateFromByteArray(binary_data.bytes, binary_data.length)) == NULL)
            {
                // Codes_SRS_UAMQP_MESSAGING_09_007: [If IoTHubMessage_CreateFromByteArray() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
                LogError("Failed creating the IOTHUB_MESSAGE_HANDLE instance (IoTHubMessage_CreateFromByteArray failed).");
                result = __FAILURE__;
            }
        }
    }

    if (iothub_message != NULL)
    {
        if (readPropertiesFromuAMQPMessage(iothub_message, uamqp_message) != RESULT_OK)
        {
            LogError("Failed reading properties of the uamqp message.");
            IoTHubMessage_Destroy(iothub_message);
            result = __FAILURE__;
        }
        else if (readApplicationPropertiesFromuAMQPMessage(iothub_message, uamqp_message) != RESULT_OK)
        {
            LogError("Failed reading application properties of the uamqp message.");
            IoTHubMessage_Destroy(iothub_message);
            result = __FAILURE__;
        }
        else
        {
            *iothubclient_message = iothub_message;
            result = RESULT_OK;
        }
    }

    return result;
}

