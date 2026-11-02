/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test gbm.h module code using gtest.
 */

#include <drm/drm_fourcc.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xf86drm.h>

#include "gbm.h"

class MockDrm
{
      public:
	MOCK_METHOD(drmVersionPtr, drmGetVersion, (int fd));
	MOCK_METHOD(void, drmFreeVersion, (drmVersionPtr v));
};

// Define a mock version of drmGetVersion
drmVersionPtr drmGetVersion(int fd)
{
	drmVersionPtr mock_version = new drmVersion();
	mock_version->name = "Mock Backend";
	return mock_version;
}

// Define a mock version of drmFreeVersion
void drmFreeVersion(drmVersionPtr v)
{
	delete (v);
}

/* TODO : This is a protocol to add unit tests for the public APIs in minigbm.
 *
 * The ultimate goal would be cover more APIs and the input combinations.
 * Set fd to 0 for now, it doesn't have any particular meaning
 */

TEST(gbm_unit_test, create_device)
{
	MockDrm mock_drm; // Create a mock object

	EXPECT_CALL(mock_drm, drmGetVersion(testing::_))
	    .WillRepeatedly(testing::Invoke(&mock_drm, &MockDrm::drmGetVersion));

	struct gbm_device *gbm_device = gbm_create_device(0);

	ASSERT_TRUE(gbm_device);

	gbm_device_destroy(gbm_device);
}

TEST(gbm_unit_test, valid_fd)
{
	MockDrm mock_drm; // Create a mock object

	EXPECT_CALL(mock_drm, drmGetVersion(testing::_))
	    .WillRepeatedly(testing::Invoke(&mock_drm, &MockDrm::drmGetVersion));
	struct gbm_device *gbm_device = gbm_create_device(99);
	int fd = gbm_device_get_fd(gbm_device);

	ASSERT_EQ(fd, 99);

	gbm_device_destroy(gbm_device);
}

TEST(gbm_unit_test, valid_backend_name)
{
	MockDrm mock_drm; // Create a mock object

	EXPECT_CALL(mock_drm, drmGetVersion(testing::_))
	    .WillRepeatedly(testing::Invoke(&mock_drm, &MockDrm::drmGetVersion));
	struct gbm_device *gbm_device = gbm_create_device(0);
	const char *backend_name = gbm_device_get_backend_name(gbm_device);

	ASSERT_STREQ(backend_name, "Mock Backend");

	gbm_device_destroy(gbm_device);
}
