// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#else
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#endif

void* real_malloc(size_t size)
{
    return malloc(size);
}

void real_free(void* ptr)
{
    free(ptr);
}

#include "testrunnerswitcher.h"
#include "azure_c_shared_utility/macro_utils.h"
#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "umocktypes_stdint.h"
#include "umock_c_negative_tests.h"
#include "umocktypes.h"
#include "umocktypes_c.h"

#define ENABLE_MOCKS
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/gballoc.h"

#include "iothub_message.h"
#include "azure_uamqp_c/message.h"

#undef ENABLE_MOCKS

#include "uamqp_messaging.h"

static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

#define TEST_IOTHUB_MESSAGE_HANDLE (IOTHUB_MESSAGE_HANDLE)0x100
#define TEST_MESSAGE_HANDLE (MESSAGE_HANDLE)0x101
#define TEST_STRING "Test string!! $%^%2F0x011"
#define TEST_MAP_HANDLE (MAP_HANDLE)0x103
#define TEST_AMQP_VALUE (AMQP_VALUE)0x104
#define TEST_PROPERTIES_HANDLE (PROPERTIES_HANDLE)0x107
#define TEST_CORRELATION_ID "Test Correlation Id"

static char** TEST_MAP_KEYS;
static char** TEST_MAP_VALUES;
static AMQP_VALUE TEST_AMQP_VALUE2 = TEST_AMQP_VALUE;
static PROPERTIES_HANDLE TEST_PROPERTIES_HANDLE_PTR = TEST_PROPERTIES_HANDLE;

static PROPERTIES_HANDLE saved_properties_get_message_id_properties;
static AMQP_VALUE saved_properties_get_message_id_message_id_value = TEST_AMQP_VALUE;
static int saved_properties_get_message_id_return = 0;

static PROPERTIES_HANDLE saved_properties_get_correlation_id_properties;
static AMQP_VALUE saved_properties_get_correlation_id_correlation_id_value = TEST_AMQP_VALUE;
static int saved_properties_get_correlation_id_return = 0;

static AMQP_VALUE saved_amqpvalue_get_string_value;
static const char* saved_amqpvalue_get_string_string_value = TEST_STRING;
static int saved_amqpvalue_get_string_return = 0;

static const char* TEST_CONTENT_TYPE = "text/plain";
static const char* TEST_CONTENT_ENCODING = "utf8";


int test_properties_get_message_id(PROPERTIES_HANDLE properties, AMQP_VALUE* message_id_value)
{
    saved_properties_get_message_id_properties = properties;
    *message_id_value = saved_properties_get_message_id_message_id_value;
    return saved_properties_get_message_id_return;
}

int test_properties_get_correlation_id(PROPERTIES_HANDLE properties, AMQP_VALUE* correlation_id_value)
{
    saved_properties_get_correlation_id_properties = properties;
    *correlation_id_value = saved_properties_get_correlation_id_correlation_id_value;
    return saved_properties_get_correlation_id_return;
}

int test_amqpvalue_get_string(AMQP_VALUE value, const char** string_value)
{
    saved_amqpvalue_get_string_value = value;
    *string_value = saved_amqpvalue_get_string_string_value;
    return saved_amqpvalue_get_string_return;
}

static void set_exp_calls_for_create_encoded_message_properties(bool has_message_id, bool has_correlation_id, const char* content_type, const char* content_encoding)
{
    size_t encoding_size = 5;

    STRICT_EXPECTED_CALL(properties_create());

    if (has_message_id)
    {
        STRICT_EXPECTED_CALL(IoTHubMessage_GetMessageId(TEST_IOTHUB_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string(TEST_STRING));
        STRICT_EXPECTED_CALL(properties_set_message_id(TEST_PROPERTIES_HANDLE, TEST_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
    }
    else
    {
        STRICT_EXPECTED_CALL(IoTHubMessage_GetMessageId(TEST_IOTHUB_MESSAGE_HANDLE)).SetReturn(NULL);
    }

    if (has_correlation_id)
    {
        STRICT_EXPECTED_CALL(IoTHubMessage_GetCorrelationId(TEST_IOTHUB_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string(TEST_CORRELATION_ID));
        STRICT_EXPECTED_CALL(properties_set_correlation_id(TEST_PROPERTIES_HANDLE, TEST_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
    }
    else
    {
        STRICT_EXPECTED_CALL(IoTHubMessage_GetCorrelationId(TEST_IOTHUB_MESSAGE_HANDLE)).SetReturn(NULL);
    }

    STRICT_EXPECTED_CALL(IoTHubMessage_GetContentTypeSystemProperty(TEST_IOTHUB_MESSAGE_HANDLE))
        .SetReturn(content_type);

    if (content_type != NULL)
    {
        STRICT_EXPECTED_CALL(properties_set_content_type(IGNORED_PTR_ARG, content_type));
    }
    
    STRICT_EXPECTED_CALL(IoTHubMessage_GetContentEncodingSystemProperty(TEST_IOTHUB_MESSAGE_HANDLE))
        .SetReturn(content_encoding);

    if (content_encoding != NULL)
    {
        STRICT_EXPECTED_CALL(properties_set_content_encoding(IGNORED_PTR_ARG, content_encoding));
    }

    STRICT_EXPECTED_CALL(amqpvalue_create_properties(TEST_PROPERTIES_HANDLE));
    STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &encoding_size, sizeof(encoding_size));
}

static void set_exp_calls_for_create_encoded_application_properties(size_t number_of_app_properties)
{
    size_t encoding_size = 5;

    STRICT_EXPECTED_CALL(IoTHubMessage_Properties(TEST_IOTHUB_MESSAGE_HANDLE));
    STRICT_EXPECTED_CALL(Map_GetInternals(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &TEST_MAP_KEYS, sizeof(TEST_MAP_KEYS))
        .CopyOutArgumentBuffer(3, &TEST_MAP_VALUES, sizeof(TEST_MAP_VALUES))
        .CopyOutArgumentBuffer(4, &number_of_app_properties, sizeof(number_of_app_properties));

    if (number_of_app_properties > 0)
    {
        STRICT_EXPECTED_CALL(amqpvalue_create_map());

        for (size_t i = 0; i < number_of_app_properties; i++)
        {
            STRICT_EXPECTED_CALL(amqpvalue_create_string(TEST_MAP_KEYS[i]));
            STRICT_EXPECTED_CALL(amqpvalue_create_string(TEST_MAP_VALUES[i]));
            STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_AMQP_VALUE, TEST_AMQP_VALUE, TEST_AMQP_VALUE));
            STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
            STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
        }

        STRICT_EXPECTED_CALL(amqpvalue_create_application_properties(TEST_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &encoding_size, sizeof(encoding_size));
    }
}

static void set_exp_calls_for_create_encoded_data(IOTHUBMESSAGE_CONTENT_TYPE msg_content_type)
{
    size_t encoding_size = 5;

    STRICT_EXPECTED_CALL(IoTHubMessage_GetContentType(TEST_IOTHUB_MESSAGE_HANDLE)).SetReturn(msg_content_type);

    if (msg_content_type == IOTHUBMESSAGE_BYTEARRAY)
    {
        STRICT_EXPECTED_CALL(IoTHubMessage_GetByteArray(TEST_IOTHUB_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    }
    else if (msg_content_type == IOTHUBMESSAGE_STRING)
    {
        STRICT_EXPECTED_CALL(IoTHubMessage_GetString(TEST_IOTHUB_MESSAGE_HANDLE));
    }

    data d;
    memset(&d, 0, sizeof(d));
    STRICT_EXPECTED_CALL(amqpvalue_create_data(d)).IgnoreArgument(1);
    STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &encoding_size, sizeof(encoding_size));
}

static void set_exp_calls_for_create_amqp_message_data(size_t number_of_app_properties, IOTHUBMESSAGE_CONTENT_TYPE msg_content_type, bool has_message_id, bool has_correlation_id, const char* content_type, const char* content_encoding)
{
    set_exp_calls_for_create_encoded_message_properties(has_message_id, has_correlation_id, content_type, content_encoding);
    set_exp_calls_for_create_encoded_application_properties(number_of_app_properties);
    set_exp_calls_for_create_encoded_data(msg_content_type);

    STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_AMQP_VALUE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    if (number_of_app_properties > 0)
    {
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_AMQP_VALUE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    }

    STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_AMQP_VALUE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
}

static void set_exp_calls_for_IoTHubMessage_CreateFromUamqpMessage(size_t number_of_properties, bool has_message_id, bool has_correlation_id, bool has_properties)
{
    static BINARY_DATA test_binary_data;
    test_binary_data.bytes = (const unsigned char*)&TEST_STRING;
    test_binary_data.length = strlen(TEST_STRING);

    MESSAGE_BODY_TYPE body_type = MESSAGE_BODY_TYPE_DATA;

    STRICT_EXPECTED_CALL(message_get_body_type(TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument_body_type()
        .CopyOutArgumentBuffer_body_type(&body_type, sizeof(MESSAGE_BODY_TYPE));
    STRICT_EXPECTED_CALL(message_get_body_amqp_data_in_place(TEST_MESSAGE_HANDLE, 0, IGNORED_PTR_ARG))
        .IgnoreArgument(3)
        .CopyOutArgumentBuffer_amqp_data(&test_binary_data, sizeof (BINARY_DATA));
    STRICT_EXPECTED_CALL(IoTHubMessage_CreateFromByteArray(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllArguments();

    // readPropertiesFromuAMQPMessage
    STRICT_EXPECTED_CALL(message_get_properties(TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument_properties()
        .CopyOutArgumentBuffer_properties(&TEST_PROPERTIES_HANDLE_PTR, sizeof(PROPERTIES_HANDLE));

    STRICT_EXPECTED_CALL(properties_get_message_id(TEST_PROPERTIES_HANDLE, IGNORED_PTR_ARG)).IgnoreArgument_message_id_value();

    if (has_message_id)
    {
        STRICT_EXPECTED_CALL(amqpvalue_get_type(TEST_AMQP_VALUE)).SetReturn(AMQP_TYPE_STRING);
        STRICT_EXPECTED_CALL(amqpvalue_get_string(TEST_AMQP_VALUE, IGNORED_PTR_ARG)).IgnoreArgument_string_value();
        STRICT_EXPECTED_CALL(IoTHubMessage_SetMessageId(TEST_IOTHUB_MESSAGE_HANDLE, TEST_STRING)).SetReturn(IOTHUB_MESSAGE_OK);
    }
    else
    {
        STRICT_EXPECTED_CALL(amqpvalue_get_type(TEST_AMQP_VALUE)).SetReturn(AMQP_TYPE_NULL);
    }

    STRICT_EXPECTED_CALL(properties_get_correlation_id(TEST_PROPERTIES_HANDLE, IGNORED_PTR_ARG)).IgnoreArgument_correlation_id_value();

    if (has_correlation_id)
    {
        STRICT_EXPECTED_CALL(amqpvalue_get_type(TEST_AMQP_VALUE)).SetReturn(AMQP_TYPE_STRING);
        STRICT_EXPECTED_CALL(amqpvalue_get_string(TEST_AMQP_VALUE, IGNORED_PTR_ARG)).IgnoreArgument_string_value();
        STRICT_EXPECTED_CALL(IoTHubMessage_SetCorrelationId(TEST_IOTHUB_MESSAGE_HANDLE, TEST_STRING)).SetReturn(IOTHUB_MESSAGE_OK);
    }
    else
    {
        STRICT_EXPECTED_CALL(amqpvalue_get_type(TEST_AMQP_VALUE)).SetReturn(AMQP_TYPE_NULL);
    }
    
    STRICT_EXPECTED_CALL(properties_destroy(TEST_PROPERTIES_HANDLE));

    // readApplicationPropertiesFromuAMQPMessage
    STRICT_EXPECTED_CALL(IoTHubMessage_Properties(TEST_IOTHUB_MESSAGE_HANDLE)).SetReturn(TEST_MAP_HANDLE);

    if (has_properties)
    {
        STRICT_EXPECTED_CALL(message_get_application_properties(TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG))
            .IgnoreArgument(2)
            .CopyOutArgumentBuffer_application_properties(&TEST_AMQP_VALUE2, sizeof(AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_get_inplace_described_value(TEST_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_get_map_pair_count(TEST_AMQP_VALUE, IGNORED_PTR_ARG))
            .IgnoreArgument(2)
            .CopyOutArgumentBuffer_pair_count((uint32_t *)&number_of_properties, sizeof(uint32_t));

        size_t i;
        for (i = 0; i < number_of_properties; i++)
        {
            STRICT_EXPECTED_CALL(amqpvalue_get_map_key_value_pair(TEST_AMQP_VALUE, (uint32_t)i, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
                .IgnoreArgument_key().IgnoreArgument_value()
                .CopyOutArgumentBuffer_key(&TEST_AMQP_VALUE2, sizeof(AMQP_VALUE))
                .CopyOutArgumentBuffer_value(&TEST_AMQP_VALUE2, sizeof(AMQP_VALUE));
            STRICT_EXPECTED_CALL(amqpvalue_get_string(TEST_AMQP_VALUE, IGNORED_PTR_ARG))
                .IgnoreArgument_string_value().CopyOutArgumentBuffer_string_value(&TEST_MAP_KEYS[i], sizeof(char*));
            STRICT_EXPECTED_CALL(amqpvalue_get_string(TEST_AMQP_VALUE, IGNORED_PTR_ARG))
                .IgnoreArgument_string_value().CopyOutArgumentBuffer_string_value(&TEST_MAP_VALUES[i], sizeof(char*));
            STRICT_EXPECTED_CALL(Map_AddOrUpdate(TEST_MAP_HANDLE, TEST_MAP_KEYS[i], TEST_MAP_VALUES[i]));
            STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
            STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
        }

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_AMQP_VALUE));
    }
    else
    {
        STRICT_EXPECTED_CALL(message_get_application_properties(TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG)).IgnoreArgument(2);
    }
}


BEGIN_TEST_SUITE(uamqp_messaging_ut)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    int result = umocktypes_charptr_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);
    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_UMOCK_ALIAS_TYPE(IOTHUB_MESSAGE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(IOTHUBMESSAGE_CONTENT_TYPE, int);
    REGISTER_UMOCK_ALIAS_TYPE(IOTHUB_MESSAGE_RESULT, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PROPERTIES_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(BINARY_DATA, void*); /*????*/
    REGISTER_UMOCK_ALIAS_TYPE(MAP_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(AMQP_VALUE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_RESULT, int);
    REGISTER_UMOCK_ALIAS_TYPE(AMQP_TYPE, int);
    REGISTER_UMOCK_ALIAS_TYPE(AMQPVALUE_ENCODER_OUTPUT, void*);
    REGISTER_UMOCK_ALIAS_TYPE(data, void*);

    REGISTER_GLOBAL_MOCK_HOOK(properties_get_message_id, test_properties_get_message_id);
    REGISTER_GLOBAL_MOCK_HOOK(properties_get_correlation_id, test_properties_get_correlation_id);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_get_string, test_amqpvalue_get_string);

    REGISTER_GLOBAL_MOCK_RETURN(message_get_properties, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_properties, 1);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_GetMessageId, TEST_STRING);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetMessageId, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(properties_set_message_id, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_set_message_id, 1);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_GetCorrelationId, TEST_CORRELATION_ID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetCorrelationId, NULL);
    
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_string, TEST_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_string, NULL);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_set_correlation_id, 1);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_set_properties, 1);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_Properties, TEST_MAP_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_Properties, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_map, TEST_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_map, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_encode, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_encode, 1);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Map_GetInternals, MAP_ERROR);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_set_map_value, 1);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_GetString, TEST_STRING);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetString, 0);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetContentType, IOTHUBMESSAGE_UNKNOWN);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetByteArray, IOTHUB_MESSAGE_ERROR);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_GetByteArray, IOTHUB_MESSAGE_OK);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_create, NULL);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_add_body_amqp_data, 1);

    REGISTER_GLOBAL_MOCK_RETURN(message_set_application_properties, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_set_application_properties, 1);

    REGISTER_GLOBAL_MOCK_RETURN(message_get_application_properties, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_application_properties, 1);
    
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_map_pair_count, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_map_pair_count, 1);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_inplace_described_value, TEST_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_inplace_described_value, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_map_key_value_pair, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_map_key_value_pair, 1);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_string, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_string, 1);
    
    REGISTER_GLOBAL_MOCK_RETURN(Map_AddOrUpdate, MAP_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Map_AddOrUpdate, MAP_ERROR);
    
    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_CreateFromByteArray, TEST_IOTHUB_MESSAGE_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_CreateFromByteArray, NULL);
        
    REGISTER_GLOBAL_MOCK_RETURN(message_get_body_type, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_body_type, 1);
    
    REGISTER_GLOBAL_MOCK_RETURN(message_get_body_amqp_data_in_place, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_body_amqp_data_in_place, 1);

    REGISTER_GLOBAL_MOCK_RETURN(properties_create, TEST_PROPERTIES_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_create, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_properties, TEST_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_properties, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_encoded_size, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_encoded_size, 1);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_data, TEST_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_data, 0);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_application_properties, TEST_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_application_properties, 0);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetContentTypeSystemProperty, NULL);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_GetContentEncodingSystemProperty, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(properties_set_content_type, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_set_content_type, 1);
    REGISTER_GLOBAL_MOCK_RETURN(properties_set_content_encoding, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_set_content_encoding, 1);
    REGISTER_GLOBAL_MOCK_RETURN(properties_get_content_type, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_get_content_type, 1);
    REGISTER_GLOBAL_MOCK_RETURN(properties_get_content_encoding, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(properties_get_content_encoding, 1);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_SetMessageId, IOTHUB_MESSAGE_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_SetMessageId, IOTHUB_MESSAGE_ERROR);
    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_SetCorrelationId, IOTHUB_MESSAGE_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_SetCorrelationId, IOTHUB_MESSAGE_ERROR);

    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_SetContentTypeSystemProperty, IOTHUB_MESSAGE_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_SetContentTypeSystemProperty, IOTHUB_MESSAGE_ERROR);
    REGISTER_GLOBAL_MOCK_RETURN(IoTHubMessage_SetContentEncodingSystemProperty, IOTHUB_MESSAGE_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(IoTHubMessage_SetContentEncodingSystemProperty, IOTHUB_MESSAGE_ERROR);
    

    // Initialization of variables.
    TEST_MAP_KEYS = (char**)real_malloc(sizeof(char*) * 5);
    ASSERT_IS_NOT_NULL_WITH_MSG(TEST_MAP_KEYS, "Could not allocate memory for TEST_MAP_KEYS");
    TEST_MAP_KEYS[0] = "PROPERTY1";
    TEST_MAP_KEYS[1] = "Property2";
    TEST_MAP_KEYS[2] = " prop(3): ";
    TEST_MAP_KEYS[3] = "A!;";
    TEST_MAP_KEYS[4] = "\r\n\t";

    TEST_MAP_VALUES = (char**)real_malloc(sizeof(char*) * 5);
    ASSERT_IS_NOT_NULL_WITH_MSG(TEST_MAP_VALUES, "Could not allocate memory for TEST_MAP_VALUES");
    TEST_MAP_VALUES[0] = "sdfksdfjjjjlsdf";
    TEST_MAP_VALUES[1] = "23,424,355,543,534,535.0";
    TEST_MAP_VALUES[2] = "@#$$$ @_=-09!!^;:";
    TEST_MAP_VALUES[3] = "     \t\r\n      ";
    TEST_MAP_VALUES[4] = "-------------";
}

TEST_SUITE_CLEANUP(TestClassCleanup)
{
    real_free(TEST_MAP_KEYS);
    real_free(TEST_MAP_VALUES);

    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(TestMethodInitialize)
{
    if (TEST_MUTEX_ACQUIRE(g_testByTest))
    {
        ASSERT_FAIL("our mutex is ABANDONED. Failure in test framework");
    }

    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

// Tests_SRS_UAMQP_MESSAGING_31_118: [Gets data associated with IOTHUB_MESSAGE_HANDLE to encode, either from underlying byte array or string format.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_120: [Create a blob that contains AMQP encoding of IOTHUB_MESSAGE_HANDLE.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_112: [If optional message-id is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_113: [If optional correlation-id is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_114: [If optional content-type is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_115: [If optional content-encoding is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_116: [Gets message properties associated with the IOTHUB_MESSAGE_HANDLE to encode, returning the properties and their encoded length.  Errors stop processing on this message.]
// Tests_SRS_UAMQP_MESSAGING_31_117: [Get application message properties associated with the IOTHUB_MESSAGE_HANDLE to encode, returning the properties and their encoded length.  Errors stop processing on this message.]
TEST_FUNCTION(create_amqp_message_data_bytearray_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_BYTEARRAY, true, true, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    // act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);

    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_117: [Get application message properties associated with the IOTHUB_MESSAGE_HANDLE to encode, returning the properties and their encoded length.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_zero_app_properties_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(0, IOTHUBMESSAGE_BYTEARRAY, true, true, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    // act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);
 
    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_118: [Gets data associated with IOTHUB_MESSAGE_HANDLE to encode, either from underlying byte array or string format.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_string_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_STRING, true, true, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    ///act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);
 
    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_112: [If optional message-id is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_no_message_id_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_STRING, false, true, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    ///act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);
 
    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_113: [If optional correlation-id is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_no_correlation_id_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_STRING, true, false, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    ///act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);

    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_114: [If optional content-type is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_no_content_type_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_STRING, true, false, NULL, TEST_CONTENT_ENCODING);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    ///act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);

    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_115: [If optional content-encoding is present in the message, encode it into the AMQP message.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_no_content_encoding_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_STRING, true, false, TEST_CONTENT_TYPE, NULL);

    BINARY_DATA binary_data;
    memset(&binary_data, 0, sizeof(binary_data));

    ///act
    int result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    //ASSERT_ARE_NOT_EQUAL(void*, NULL, binary_data.bytes);

    // cleanup
    if (NULL != binary_data.bytes)
    {
        real_free((void*)binary_data.bytes);
    }
}

// Tests_SRS_UAMQP_MESSAGING_31_120: [Create a blob that contains AMQP encoding of IOTHUB_MESSAGE_HANDLE.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_BYTEARRAY_return_errors_fails)
{
    // arrange
    int result = 0;
    result = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, result);
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_BYTEARRAY, true, true, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    umock_c_negative_tests_snapshot();

    // act
    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        // arrange
        char error_msg[64];

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        if ((i == 4) || // amqpvalue_destroy
             (i == 8) || // amqpvalue_destroy
             (i == 19) || // amqpvalue_destroy
             (i == 21) || // amqpvalue_destroy
             (i == 22) // amqpvalue_destroy
           )
        {
            continue; // these lines have functions that do not return anything (void).
        }

        BINARY_DATA binary_data;
        memset(&binary_data, 0, sizeof(binary_data));

        result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

        // assert
        if ((i == 1) || // GetMessageId is optional
            (i == 5) || //GetCorrelationId is optional
            (i == 9) || // ContentType is optional 
            (i == 11)   // ContentEncoding is optional
           )
        {
            ASSERT_ARE_EQUAL(int, result, 0);
        }
        else
        {
            sprintf(error_msg, "On failed call %zu", i);
            ASSERT_ARE_NOT_EQUAL_WITH_MSG(int, result, 0, error_msg);
        }

        if (NULL != binary_data.bytes)
        {
            real_free((void*)binary_data.bytes);
        }
    }

    // cleanup
    umock_c_negative_tests_reset();
    umock_c_negative_tests_deinit();
}

// Tests_SRS_UAMQP_MESSAGING_31_118: [Gets data associated with IOTHUB_MESSAGE_HANDLE to encode, either from underlying byte array or string format.  Errors stop processing on this message.]
TEST_FUNCTION(message_create_from_iothub_message_STRING_return_errors_fails)
{
    // arrange
    int result = 0;
    result = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, result);
    umock_c_reset_all_calls();
    set_exp_calls_for_create_amqp_message_data(1, IOTHUBMESSAGE_STRING, true, true, TEST_CONTENT_TYPE, TEST_CONTENT_ENCODING);

    umock_c_negative_tests_snapshot();

    // act
    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        // arrange
        char error_msg[64];

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        if ((i == 4) || // amqpvalue_destroy
             (i == 8) || // amqpvalue_destroy
             (i == 19) || // amqpvalue_destroy
             (i == 21) || // amqpvalue_destroy
             (i == 22) // amqpvalue_destroy
           )
        {
            continue; // these lines have functions that do not return anything (void).
        }

        BINARY_DATA binary_data;
        memset(&binary_data, 0, sizeof(binary_data));

        result = create_amqp_message_data(TEST_IOTHUB_MESSAGE_HANDLE, &binary_data);

        // assert
        if ((i == 1) || // GetMessageId is optional
            (i == 5) || //GetCorrelationId is optional
            (i == 9) || // ContentType is optional 
            (i == 11)   // ContentEncoding is optional
             )
        {
            ASSERT_ARE_EQUAL(int, result, 0);
        }
        else
        {
            sprintf(error_msg, "On failed call %zu", i);
            ASSERT_ARE_NOT_EQUAL_WITH_MSG(int, result, 0, error_msg);
        }

        if (NULL != binary_data.bytes)
        {
            real_free((void*)binary_data.bytes);
        }
    }

    // cleanup
    umock_c_negative_tests_reset();
    umock_c_negative_tests_deinit();
}

// Tests_SRS_UAMQP_MESSAGING_09_001: [The body type of the uAMQP message shall be retrieved using message_get_body_type().]
// Tests_SRS_UAMQP_MESSAGING_09_003: [If the uAMQP message body type is MESSAGE_BODY_TYPE_DATA, the body data shall be treated as binary data.]
// Tests_SRS_UAMQP_MESSAGING_09_004: [The uAMQP message body data shall be retrieved using message_get_body_amqp_data_in_place().]
// Tests_SRS_UAMQP_MESSAGING_09_006: [The IOTHUB_MESSAGE instance shall be created using IoTHubMessage_CreateFromByteArray(), passing the uAMQP body bytes as parameter.]
// Tests_SRS_UAMQP_MESSAGING_09_008: [The uAMQP message properties shall be retrieved using message_get_properties().]
// Tests_SRS_UAMQP_MESSAGING_09_010: [The message-id property shall be read from the uAMQP message by calling properties_get_message_id.]
// Tests_SRS_UAMQP_MESSAGING_09_012: [The type of the message-id property value shall be obtained using amqpvalue_get_type().]
// Tests_SRS_UAMQP_MESSAGING_09_014: [The message-id value shall be retrieved from the AMQP_VALUE as char* by calling amqpvalue_get_string().]
// Tests_SRS_UAMQP_MESSAGING_09_016: [The message-id property shall be set on the IOTHUB_MESSAGE_HANDLE instance by calling IoTHubMessage_SetMessageId(), passing the value read from the uAMQP message.]
// Tests_SRS_UAMQP_MESSAGING_09_018: [The correlation-id property shall be read from the uAMQP message by calling properties_get_correlation_id.]
// Tests_SRS_UAMQP_MESSAGING_09_020: [The type of the correlation-id property value shall be obtained using amqpvalue_get_type().]
// Tests_SRS_UAMQP_MESSAGING_09_022: [The correlation-id value shall be retrieved from the AMQP_VALUE as char* by calling amqpvalue_get_string.]
// Tests_SRS_UAMQP_MESSAGING_09_024: [The correlation-id property shall be set on the IOTHUB_MESSAGE_HANDLE by calling IoTHubMessage_SetCorrelationId, passing the value read from the uAMQP message.]
// Tests_SRS_UAMQP_MESSAGING_09_026: [IoTHubMessage_CreateFromuAMQPMessage() shall destroy the uAMQP message properties (obtained with message_get_properties()) by calling properties_destroy().]
// Tests_SRS_UAMQP_MESSAGING_09_027: [The IOTHUB_MESSAGE_HANDLE properties shall be retrieved using IoTHubMessage_Properties.]
// Tests_SRS_UAMQP_MESSAGING_09_029: [The uAMQP message application properties shall be retrieved using message_get_application_properties.]
// Tests_SRS_UAMQP_MESSAGING_09_032: [The actual uAMQP message application properties should be extracted from the result of message_get_application_properties using amqpvalue_get_inplace_described_value.]
// Tests_SRS_UAMQP_MESSAGING_09_034: [The number of items in the uAMQP message application properties shall be obtained using amqpvalue_get_map_pair_count.]
// Tests_SRS_UAMQP_MESSAGING_09_036: [IoTHubMessage_CreateFromuAMQPMessage() shall iterate through each uAMQP application property and add it to IOTHUB_MESSAGE_HANDLE properties.]
// Tests_SRS_UAMQP_MESSAGING_09_037: [The uAMQP application property name and value shall be obtained using amqpvalue_get_map_key_value_pair.]
// Tests_SRS_UAMQP_MESSAGING_09_039: [The uAMQP application property name shall be extracted as string using amqpvalue_get_string.]
// Tests_SRS_UAMQP_MESSAGING_09_041: [The uAMQP application property value shall be extracted as string using amqpvalue_get_string.]
// Tests_SRS_UAMQP_MESSAGING_09_043: [The application property name and value shall be added to IOTHUB_MESSAGE_HANDLE properties using Map_AddOrUpdate.]
// Tests_SRS_UAMQP_MESSAGING_09_045: [IoTHubMessage_CreateFromuAMQPMessage() shall destroy the uAMQP message property name and value (obtained with amqpvalue_get_string) by calling amqpvalue_destroy().]
// Tests_SRS_UAMQP_MESSAGING_09_046: [IoTHubMessage_CreateFromuAMQPMessage() shall destroy the uAMQP message property (obtained with message_get_application_properties) by calling amqpvalue_destroy().]
TEST_FUNCTION(IoTHubMessage_CreateFromUamqpMessage_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_IoTHubMessage_CreateFromUamqpMessage(1, true, true, true);

    // act
    IOTHUB_MESSAGE_HANDLE iothub_client_message = NULL;
    int result = IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, &iothub_client_message);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    ASSERT_ARE_EQUAL(void_ptr, (void*)iothub_client_message, (void*)TEST_IOTHUB_MESSAGE_HANDLE);

    // cleanup
}

// Tests_SRS_UAMQP_MESSAGING_09_013: [If the type of the message-id property value is AMQP_TYPE_NULL, IoTHubMessage_CreateFromuAMQPMessage() shall skip processing the message-id (as it is optional) and continue normally.]
TEST_FUNCTION(IoTHubMessage_CreateFromUamqpMessage_no_message_id_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_IoTHubMessage_CreateFromUamqpMessage(1, false, true, true);

    // act
    IOTHUB_MESSAGE_HANDLE iothub_client_message = NULL;
    int result = IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, &iothub_client_message);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    ASSERT_ARE_EQUAL(void_ptr, (void*)iothub_client_message, (void*)TEST_IOTHUB_MESSAGE_HANDLE);

    // cleanup
}

// Tests_SRS_UAMQP_MESSAGING_09_021: [If the type of the correlation-id property value is AMQP_TYPE_NULL, IoTHubMessage_CreateFromuAMQPMessage() shall skip processing the correlation-id (as it is optional) and continue normally.]
TEST_FUNCTION(IoTHubMessage_CreateFromUamqpMessage_no_correlation_id_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_IoTHubMessage_CreateFromUamqpMessage(1, true, false, true);

    // act
    IOTHUB_MESSAGE_HANDLE iothub_client_message = NULL;
    int result = IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, &iothub_client_message);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    ASSERT_ARE_EQUAL(void_ptr, (void*)iothub_client_message, (void*)TEST_IOTHUB_MESSAGE_HANDLE);

    // cleanup
}

// Tests_SRS_UAMQP_MESSAGING_09_002: [If message_get_body_type() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_005: [If message_get_body_amqp_data_in_place() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_007: [If IoTHubMessage_CreateFromByteArray() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_009: [If message_get_properties() fails, IoTHubMessage_CreateFromuAMQPMessage shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_011: [If properties_get_message_id fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_015: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_017: [If IoTHubMessage_SetMessageId fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_019: [If properties_get_correlation_id() fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_023: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_025: [If IoTHubMessage_SetCorrelationId fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_028: [If IoTHubMessage_Properties fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_030: [If message_get_application_properties fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_033: [If amqpvalue_get_inplace_described_value fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_035: [If amqpvalue_get_map_pair_count fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_038: [If amqpvalue_get_map_key_value_pair fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_040: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_042: [If amqpvalue_get_string fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
// Tests_SRS_UAMQP_MESSAGING_09_044: [If Map_AddOrUpdate fails, IoTHubMessage_CreateFromuAMQPMessage() shall fail and return immediately.]
TEST_FUNCTION(IoTHubMessage_CreateFromUamqpMessage_error_returns_fails)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_IoTHubMessage_CreateFromUamqpMessage(1, true, true, true);

    // act
    IOTHUB_MESSAGE_HANDLE iothub_client_message = NULL;
    int result = IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, &iothub_client_message);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    ASSERT_ARE_EQUAL(void_ptr, (void*)iothub_client_message, (void*)TEST_IOTHUB_MESSAGE_HANDLE);

    // cleanup
}

// Tests_SRS_UAMQP_MESSAGING_09_031: [If message_get_application_properties succeeds but returns a NULL application properties map (there are no properties), IoTHubMessage_CreateFromuAMQPMessage() shall skip processing the properties and continue normally.]
TEST_FUNCTION(IoTHubMessage_CreateFromUamqpMessage_no_app_properties_success)
{
    // arrange
    umock_c_reset_all_calls();
    set_exp_calls_for_IoTHubMessage_CreateFromUamqpMessage(0, true, true, false);

    // act
    IOTHUB_MESSAGE_HANDLE iothub_client_message = NULL;
    int result = IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, &iothub_client_message);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
    ASSERT_ARE_EQUAL(void_ptr, (void*)iothub_client_message, (void*)TEST_IOTHUB_MESSAGE_HANDLE);

    // cleanup
}

END_TEST_SUITE(uamqp_messaging_ut)

