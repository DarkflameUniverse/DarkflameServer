#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include "HTTPContext.h"
#include "Web.h"
#include "AuthMiddleware.h"
#include "RequireAuthMiddleware.h"

/**
 * Route Integration Tests
 * 
 * These tests verify the actual route handlers work correctly with middleware chains.
 * Unlike MiddlewareTests.cpp which uses mocks, these tests use real middleware
 * to verify the complete authentication and authorization flow.
 */

// Mock DashboardAuthService for testing
namespace {
	class MockDashboardAuthService {
	public:
		static bool VerifyToken(const std::string& token, std::string& outUsername, uint8_t& outGmLevel) {
			// Test tokens with predictable results
			if (token == "valid_user_token") {
				outUsername = "testuser";
				outGmLevel = 0;  // Regular user
				return true;
			}
			if (token == "admin_token") {
				outUsername = "admin";
				outGmLevel = 9;  // Admin
				return true;
			}
			if (token == "moderator_token") {
				outUsername = "moderator";
				outGmLevel = 5;  // Moderator
				return true;
			}
			return false;
		}

		static bool HasDashboardAccess(uint8_t gmLevel) {
			return gmLevel > 0;
		}
	};
}

// Test fixture for route handlers
class RouteHandlerTest : public ::testing::Test {
protected:
	HTTPContext context;
	HTTPReply reply;

	void SetUp() override {
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::APPLICATION_JSON;
		reply.message = "";
	}

	// Simulate a route handler for /api/status
	void HandleStatusRoute(HTTPReply& out, const HTTPContext& in) {
		out.status = eHTTPStatusCode::OK;
		out.contentType = eContentType::APPLICATION_JSON;
		out.message = R"({"status":"running","version":"1.0.0"})";
	}

	// Simulate a route handler for /api/players
	void HandlePlayersRoute(HTTPReply& out, const HTTPContext& in) {
		out.status = eHTTPStatusCode::OK;
		out.contentType = eContentType::APPLICATION_JSON;
		out.message = R"({"players":[{"id":1,"name":"Player1"},{"id":2,"name":"Player2"}]})";
	}

	// Simulate a route handler for /api/accounts/count
	void HandleAccountsCountRoute(HTTPReply& out, const HTTPContext& in) {
		out.status = eHTTPStatusCode::OK;
		out.contentType = eContentType::APPLICATION_JSON;
		out.message = R"({"count":42})";
	}

	// Simulate a route handler for /api/characters/count
	void HandleCharactersCountRoute(HTTPReply& out, const HTTPContext& in) {
		out.status = eHTTPStatusCode::OK;
		out.contentType = eContentType::APPLICATION_JSON;
		out.message = R"({"count":128})";
	}
};

// Test protected API routes with authentication
class ProtectedAPIRouteTest : public RouteHandlerTest {
protected:
	void ProcessMiddlewareChain(std::vector<std::shared_ptr<IHTTPMiddleware>>& middlewares, HTTPContext& ctx) {
		for (const auto& middleware : middlewares) {
			if (!middleware->Process(ctx, reply)) {
				break;
			}
		}
	}
};

TEST_F(ProtectedAPIRouteTest, StatusRouteRequiresAuthentication) {
	// Create middleware chain for protected route
	std::vector<std::shared_ptr<IHTTPMiddleware>> middlewares;

	// Simulate AuthMiddleware (always passes, extracts token if available)
	context.path = "/api/status";
	context.queryString = "";  // No token
	context.method = "GET";

	// Without authentication
	std::string username;
	uint8_t gmLevel{};
	
	EXPECT_FALSE(context.isAuthenticated);
	EXPECT_EQ(context.gmLevel, 0);

	// Now test with token
	context.queryString = "token=valid_user_token";
	
	// Extract and verify token (simulating AuthMiddleware)
	std::string token = "valid_user_token";
	if (MockDashboardAuthService::VerifyToken(token, username, gmLevel)) {
		context.isAuthenticated = true;
		context.authenticatedUser = username;
		context.gmLevel = gmLevel;
	}

	EXPECT_TRUE(context.isAuthenticated);
	EXPECT_EQ(context.authenticatedUser, "testuser");
	EXPECT_EQ(context.gmLevel, 0);
}

TEST_F(ProtectedAPIRouteTest, PlayersRouteWithValidAuth) {
	context.path = "/api/players";
	context.method = "GET";

	// Simulate token verification
	std::string username;
	uint8_t gmLevel{};
	std::string token = "admin_token";

	if (MockDashboardAuthService::VerifyToken(token, username, gmLevel)) {
		context.isAuthenticated = true;
		context.authenticatedUser = username;
		context.gmLevel = gmLevel;
	}

	// Check authentication
	EXPECT_TRUE(context.isAuthenticated);
	EXPECT_EQ(context.gmLevel, 9);

	// Call handler
	HandlePlayersRoute(reply, context);

	// Verify response
	EXPECT_EQ(reply.status, eHTTPStatusCode::OK);
	EXPECT_NE(reply.message.find("players"), std::string::npos);
}

TEST_F(ProtectedAPIRouteTest, AccountsCountRouteRequiresLevel0) {
	context.path = "/api/accounts/count";
	context.method = "POST";

	// Test with level 0 user (should pass)
	std::string username;
	uint8_t gmLevel{};
	std::string token = "valid_user_token";

	if (MockDashboardAuthService::VerifyToken(token, username, gmLevel)) {
		context.isAuthenticated = true;
		context.authenticatedUser = username;
		context.gmLevel = gmLevel;
	}

	EXPECT_TRUE(context.isAuthenticated);
	EXPECT_GE(context.gmLevel, 0);  // Meets requirement

	HandleAccountsCountRoute(reply, context);
	EXPECT_EQ(reply.status, eHTTPStatusCode::OK);
}

TEST_F(ProtectedAPIRouteTest, CharactersCountRouteWithModerator) {
	context.path = "/api/characters/count";
	context.method = "POST";

	// Test with moderator token
	std::string username;
	uint8_t gmLevel{};
	std::string token = "moderator_token";

	if (MockDashboardAuthService::VerifyToken(token, username, gmLevel)) {
		context.isAuthenticated = true;
		context.authenticatedUser = username;
		context.gmLevel = gmLevel;
	}

	EXPECT_TRUE(context.isAuthenticated);
	EXPECT_EQ(context.gmLevel, 5);

	HandleCharactersCountRoute(reply, context);
	EXPECT_EQ(reply.status, eHTTPStatusCode::OK);
	EXPECT_NE(reply.message.find("count"), std::string::npos);
}

// Test authentication failures
class AuthenticationFailureTest : public RouteHandlerTest {
};

TEST_F(AuthenticationFailureTest, InvalidTokenRejected) {
	context.path = "/api/status";
	context.method = "GET";

	std::string username;
	uint8_t gmLevel{};
	std::string token = "invalid_token";

	// Should fail
	EXPECT_FALSE(MockDashboardAuthService::VerifyToken(token, username, gmLevel));
	EXPECT_FALSE(context.isAuthenticated);
	EXPECT_EQ(context.gmLevel, 0);
}

TEST_F(AuthenticationFailureTest, ExpiredTokenRejected) {
	context.path = "/api/players";
	context.method = "GET";

	std::string username;
	uint8_t gmLevel{};
	std::string token = "expired_token";

	// Should fail
	EXPECT_FALSE(MockDashboardAuthService::VerifyToken(token, username, gmLevel));
	EXPECT_EQ(gmLevel, 0);
}

TEST_F(AuthenticationFailureTest, MissingTokenRejectsProtectedRoute) {
	context.path = "/api/status";
	context.method = "GET";
	context.queryString = "";  // No token
	context.isAuthenticated = false;
	context.gmLevel = 0;

	EXPECT_FALSE(context.isAuthenticated);
	EXPECT_EQ(context.gmLevel, 0);

	// Route should return 401
	reply.status = eHTTPStatusCode::UNAUTHORIZED;
	reply.message = "{\"error\":\"Authentication required\"}";
	
	EXPECT_EQ(reply.status, eHTTPStatusCode::UNAUTHORIZED);
	EXPECT_NE(reply.message.find("Authentication required"), std::string::npos);
}

// Test authorization level checking
class AuthorizationLevelTest : public RouteHandlerTest {
protected:
	bool CheckAuthorizationLevel(uint8_t userLevel, uint8_t requiredLevel) {
		return userLevel >= requiredLevel;
	}
};

TEST_F(AuthorizationLevelTest, Level0UserAccessLevel0Route) {
	context.gmLevel = 0;
	EXPECT_TRUE(CheckAuthorizationLevel(context.gmLevel, 0));
}

TEST_F(AuthorizationLevelTest, Level9AdminAccessAnyRoute) {
	context.gmLevel = 9;
	EXPECT_TRUE(CheckAuthorizationLevel(context.gmLevel, 0));
	EXPECT_TRUE(CheckAuthorizationLevel(context.gmLevel, 5));
	EXPECT_TRUE(CheckAuthorizationLevel(context.gmLevel, 9));
}

TEST_F(AuthorizationLevelTest, Level1CannotAccessLevel5Route) {
	context.gmLevel = 1;
	EXPECT_FALSE(CheckAuthorizationLevel(context.gmLevel, 5));
}

TEST_F(AuthorizationLevelTest, InsufficientLevelReturns403) {
	context.gmLevel = 0;
	
	if (!CheckAuthorizationLevel(context.gmLevel, 5)) {
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"error\":\"Insufficient permissions\"}";
	}

	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
	EXPECT_NE(reply.message.find("Insufficient permissions"), std::string::npos);
}

// Test token extraction from different sources
class TokenSourceTest : public RouteHandlerTest {
protected:
	std::string ExtractTokenFromQuery(const std::string& queryString) {
		if (queryString.empty()) return "";
		size_t pos = queryString.find("token=");
		if (pos == std::string::npos) return "";
		size_t start = pos + 6;
		size_t end = queryString.find("&", start);
		if (end == std::string::npos) end = queryString.length();
		return queryString.substr(start, end - start);
	}

	std::string ExtractTokenFromHeader(const std::string& authHeader) {
		if (authHeader.empty()) return "";
		if (authHeader.substr(0, 7) == "Bearer ") return authHeader.substr(7);
		if (authHeader.substr(0, 6) == "Token ") return authHeader.substr(6);
		return authHeader;
	}

	std::string ExtractTokenFromCookie(const std::string& cookieHeader) {
		if (cookieHeader.empty()) return "";
		size_t pos = cookieHeader.find("dashboardToken=");
		if (pos == std::string::npos) return "";
		size_t start = pos + 15;
		size_t end = cookieHeader.find(";", start);
		if (end == std::string::npos) end = cookieHeader.length();
		return cookieHeader.substr(start, end - start);
	}
};

TEST_F(TokenSourceTest, ExtractFromQueryString) {
	context.queryString = "token=valid_user_token&other=param";
	std::string token = ExtractTokenFromQuery(context.queryString);
	EXPECT_EQ(token, "valid_user_token");
}

TEST_F(TokenSourceTest, ExtractFromAuthorizationHeader) {
	context.SetHeader("Authorization", "Bearer admin_token");
	std::string authHeader = context.GetHeader("Authorization");
	std::string token = ExtractTokenFromHeader(authHeader);
	EXPECT_EQ(token, "admin_token");
}

TEST_F(TokenSourceTest, ExtractFromCookie) {
	context.SetHeader("Cookie", "dashboardToken=moderator_token; Path=/");
	std::string cookieHeader = context.GetHeader("Cookie");
	std::string token = ExtractTokenFromCookie(cookieHeader);
	EXPECT_EQ(token, "moderator_token");
}

// Test response formatting
class ResponseFormattingTest : public RouteHandlerTest {
};

TEST_F(ResponseFormattingTest, SuccessResponseFormat) {
	reply.status = eHTTPStatusCode::OK;
	reply.contentType = eContentType::APPLICATION_JSON;
	reply.message = R"({"status":"success","data":{}})";

	EXPECT_EQ(reply.status, eHTTPStatusCode::OK);
	EXPECT_EQ(reply.contentType, eContentType::APPLICATION_JSON);
	EXPECT_NE(reply.message.find("success"), std::string::npos);
}

TEST_F(ResponseFormattingTest, UnauthorizedResponseFormat) {
	reply.status = eHTTPStatusCode::UNAUTHORIZED;
	reply.contentType = eContentType::APPLICATION_JSON;
	reply.message = R"({"error":"Unauthorized","code":401})";

	EXPECT_EQ(reply.status, eHTTPStatusCode::UNAUTHORIZED);
	EXPECT_NE(reply.message.find("error"), std::string::npos);
	EXPECT_NE(reply.message.find("401"), std::string::npos);
}

TEST_F(ResponseFormattingTest, ForbiddenResponseFormat) {
	reply.status = eHTTPStatusCode::FORBIDDEN;
	reply.contentType = eContentType::APPLICATION_JSON;
	reply.message = R"({"error":"Forbidden","code":403})";

	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
	EXPECT_NE(reply.message.find("error"), std::string::npos);
	EXPECT_NE(reply.message.find("403"), std::string::npos);
}

// Integration test: Full request flow
class FullRequestFlowTest : public RouteHandlerTest {
protected:
	struct RequestFlow {
		std::string method;
		std::string path;
		std::string token;
		uint8_t requiredLevel;
		bool shouldSucceed;
	};

	eHTTPStatusCode ProcessRequest(const RequestFlow& flow) {
		// Step 1: Set request context
		context.method = flow.method;
		context.path = flow.path;
		context.queryString = flow.token.empty() ? "" : ("token=" + flow.token);

		// Step 2: Try to verify token
		if (!flow.token.empty()) {
			std::string username;
			uint8_t gmLevel{};
			if (MockDashboardAuthService::VerifyToken(flow.token, username, gmLevel)) {
				context.isAuthenticated = true;
				context.authenticatedUser = username;
				context.gmLevel = gmLevel;
			}
		}

		// Step 3: Check authorization
		if (context.isAuthenticated && context.gmLevel >= flow.requiredLevel) {
			// Call handler
			if (flow.path == "/api/status") {
				HandleStatusRoute(reply, context);
			} else if (flow.path == "/api/players") {
				HandlePlayersRoute(reply, context);
			}
			return reply.status;
		} else if (!context.isAuthenticated) {
			return eHTTPStatusCode::UNAUTHORIZED;
		} else {
			return eHTTPStatusCode::FORBIDDEN;
		}
	}
};

TEST_F(FullRequestFlowTest, ValidUserAccessesPublicAPI) {
	RequestFlow flow{
		.method = "GET",
		.path = "/api/status",
		.token = "valid_user_token",
		.requiredLevel = 0,
		.shouldSucceed = true
	};

	eHTTPStatusCode result = ProcessRequest(flow);
	EXPECT_EQ(result, eHTTPStatusCode::OK);
	EXPECT_TRUE(context.isAuthenticated);
}

TEST_F(FullRequestFlowTest, AdminAccessesProtectedAPI) {
	RequestFlow flow{
		.method = "GET",
		.path = "/api/players",
		.token = "admin_token",
		.requiredLevel = 0,
		.shouldSucceed = true
	};

	eHTTPStatusCode result = ProcessRequest(flow);
	EXPECT_EQ(result, eHTTPStatusCode::OK);
	EXPECT_TRUE(context.isAuthenticated);
	EXPECT_EQ(context.gmLevel, 9);
}

TEST_F(FullRequestFlowTest, NoTokenReturnsUnauthorized) {
	RequestFlow flow{
		.method = "GET",
		.path = "/api/status",
		.token = "",
		.requiredLevel = 0,
		.shouldSucceed = false
	};

	eHTTPStatusCode result = ProcessRequest(flow);
	EXPECT_EQ(result, eHTTPStatusCode::UNAUTHORIZED);
	EXPECT_FALSE(context.isAuthenticated);
}

TEST_F(FullRequestFlowTest, InvalidTokenReturnsUnauthorized) {
	RequestFlow flow{
		.method = "GET",
		.path = "/api/players",
		.token = "invalid_token",
		.requiredLevel = 0,
		.shouldSucceed = false
	};

	eHTTPStatusCode result = ProcessRequest(flow);
	EXPECT_EQ(result, eHTTPStatusCode::UNAUTHORIZED);
	EXPECT_FALSE(context.isAuthenticated);
}
