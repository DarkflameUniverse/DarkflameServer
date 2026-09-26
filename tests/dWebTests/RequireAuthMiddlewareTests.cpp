#include <gtest/gtest.h>
#include "RequireAuthMiddleware.h"
#include "HTTPContext.h"
#include "Web.h"

class RequireAuthMiddlewareTest : public ::testing::Test {
protected:
	HTTPContext context;
	HTTPReply reply;

	void Authenticate(uint8_t gmLevel, const std::string& source) {
		context.isAuthenticated = true;
		context.authenticatedUser = "tester";
		context.gmLevel = gmLevel;
		context.userData["auth_source"] = source;
	}
};

TEST_F(RequireAuthMiddlewareTest, UnauthenticatedApiRequestGets401) {
	context.method = "GET";
	context.path = "/api/status";
	EXPECT_FALSE(RequireAuthMiddleware(0).Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::UNAUTHORIZED);
}

TEST_F(RequireAuthMiddlewareTest, UnauthenticatedPageRedirectsToLogin) {
	context.method = "GET";
	context.path = "/accounts";
	EXPECT_FALSE(RequireAuthMiddleware(1).Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::FOUND);
	EXPECT_EQ(reply.location, "/login");
}

TEST_F(RequireAuthMiddlewareTest, InsufficientLevelIsForbidden) {
	context.method = "GET";
	context.path = "/api/tables/audit_log";
	Authenticate(4, "header");
	EXPECT_FALSE(RequireAuthMiddleware(8).Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
}

TEST_F(RequireAuthMiddlewareTest, CookiePostWithoutCsrfHeaderIsRejected) {
	context.method = "POST";
	context.path = "/api/accounts/2/ban";
	Authenticate(9, "cookie");
	EXPECT_FALSE(RequireAuthMiddleware(4).Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
}

TEST_F(RequireAuthMiddlewareTest, CookiePostWithCsrfHeaderIsAllowed) {
	context.method = "POST";
	context.path = "/api/accounts/2/ban";
	context.SetHeader("X-Requested-With", "dashboard");
	Authenticate(9, "cookie");
	EXPECT_TRUE(RequireAuthMiddleware(4).Process(context, reply));
}

TEST_F(RequireAuthMiddlewareTest, BearerPostDoesNotNeedCsrfHeader) {
	context.method = "POST";
	context.path = "/api/accounts/2/ban";
	Authenticate(9, "header");
	EXPECT_TRUE(RequireAuthMiddleware(4).Process(context, reply));
}

TEST_F(RequireAuthMiddlewareTest, CookieGetDoesNotNeedCsrfHeader) {
	context.method = "GET";
	context.path = "/api/status";
	Authenticate(0, "cookie");
	EXPECT_TRUE(RequireAuthMiddleware(0).Process(context, reply));
}
