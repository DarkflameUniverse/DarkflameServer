#include <fstream>
#include <memory>
#include <gtest/gtest.h>

#include "AMFDeserialize.h"
#include "Amf3.h"

#include "Game.h"
#include "dLogger.h"

// This is needed to prevent linker errors with the external dLogger class in Game.h.
// I need to put the full namespace definition in a separate cpp file with a header and a Test_F for all
// tests so they properly delete this memory.  So yes, right now this leaks memory.
namespace Game {
	dLogger* logger = new dLogger("testing.log", true, true);
}

/**
 * Helper method that all tests use to get their respective AMF.
 */
AMFBaseValue* ReadFromBitStream(RakNet::BitStream* bitStream) {
	AMFDeserialize deserializer;
	AMFBaseValue* returnValue(deserializer.Read(bitStream));
	return returnValue;
}

/**
 * @brief Test reading an AMFUndefined value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFUndefinedTest) {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x00);
	std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), eAmf::Undefined);
}

/**
 * @brief Test reading an AMFNull value from a BitStream.
 *
 */
TEST(dCommonTests, AMFDeserializeAMFNullTest) {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x01);
	std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), eAmf::Null);
}

/**
 * @brief Test reading an AMFFalse value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFFalseTest) {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x02);
	std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), eAmf::False);
}

/**
 * @brief Test reading an AMFTrue value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFTrueTest) {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x03);
	std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), eAmf::True);
}

/**
 * @brief Test reading an AMFInteger value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFIntegerTest) {
	CBITSTREAM;
	{
		bitStream.Write<uint8_t>(0x04);
		// 127 == 01111111
		bitStream.Write<uint8_t>(127);
		std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), eAmf::Integer);
		// Check that the max value of a byte can be read correctly
		ASSERT_EQ(static_cast<AMFIntValue*>(res.get())->GetValue(), 127);
	}
	bitStream.Reset();
	{
		bitStream.Write<uint8_t>(0x04);
		bitStream.Write<uint32_t>(UINT32_MAX);
		std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), eAmf::Integer);
		// Check that we can read the maximum value correctly
		ASSERT_EQ(static_cast<AMFIntValue*>(res.get())->GetValue(), 536870911);
	}
	bitStream.Reset();
	{
		bitStream.Write<uint8_t>(0x04);
		// 131 == 10000011
		bitStream.Write<uint8_t>(131);
		// 255 == 11111111
		bitStream.Write<uint8_t>(255);
		// 127 == 01111111
		bitStream.Write<uint8_t>(127);
		std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), eAmf::Integer);
		// Check that short max can be read correctly
		ASSERT_EQ(static_cast<AMFIntValue*>(res.get())->GetValue(), UINT16_MAX);
	}
	bitStream.Reset();
	{
		bitStream.Write<uint8_t>(0x04);
		// 255 == 11111111
		bitStream.Write<uint8_t>(255);
		// 127 == 01111111
		bitStream.Write<uint8_t>(127);
		std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), eAmf::Integer);
		// Check that 2 byte max can be read correctly
		ASSERT_EQ(static_cast<AMFIntValue*>(res.get())->GetValue(), 16383);
	}
}

/**
 * @brief Test reading an AMFDouble value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFDoubleTest) {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x05);
	bitStream.Write<double>(25346.4f);
	std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), eAmf::Double);
	ASSERT_EQ(static_cast<AMFDoubleValue*>(res.get())->GetValue(), 25346.4f);
}

/**
 * @brief Test reading an AMFString value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFStringTest) {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x06);
	bitStream.Write<uint8_t>(0x0F);
	std::string toWrite = "stateID";
	for (auto e : toWrite) bitStream.Write<char>(e);
	std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), eAmf::String);
	ASSERT_EQ(static_cast<AMFStringValue*>(res.get())->GetValue(), "stateID");
}

/**
 * @brief Test reading an AMFArray value from a BitStream.
 */
TEST(dCommonTests, AMFDeserializeAMFArrayTest) {
	CBITSTREAM;
	// Test empty AMFArray
	bitStream.Write<uint8_t>(0x09);
	bitStream.Write<uint8_t>(0x01);
	bitStream.Write<uint8_t>(0x01);
	{
		std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), eAmf::Array);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetAssociative().size(), 0);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetDense().size(), 0);
	}
	bitStream.Reset();
	// Test a key'd value and dense value
	bitStream.Write<uint8_t>(0x09);
	bitStream.Write<uint8_t>(0x03);
	bitStream.Write<uint8_t>(0x15);
	for (auto e : "BehaviorID") if (e != '\0') bitStream.Write<char>(e);
	bitStream.Write<uint8_t>(0x06);
	bitStream.Write<uint8_t>(0x0B);
	for (auto e : "10447") if (e != '\0') bitStream.Write<char>(e);
	bitStream.Write<uint8_t>(0x01);
	bitStream.Write<uint8_t>(0x06);
	bitStream.Write<uint8_t>(0x0B);
	for (auto e : "10447") if (e != '\0') bitStream.Write<char>(e);
	{
		std::unique_ptr<AMFBaseValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), eAmf::Array);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetAssociative().size(), 1);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetDense().size(), 1);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->Get<std::string>("BehaviorID")->GetValue(), "10447");
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->Get<std::string>(0)->GetValue(), "10447");
	}
}

/**
 * @brief This test checks that if we recieve an unimplemented eAmf
 * we correctly throw an error and can actch it.
 * Yes this leaks memory.  
 */
TEST(dCommonTests, AMFDeserializeUnimplementedValuesTest) {
	std::vector<eAmf> unimplementedValues = {
		eAmf::XMLDoc,
		eAmf::Date,
		eAmf::Object,
		eAmf::XML,
		eAmf::ByteArray,
		eAmf::VectorInt,
		eAmf::VectorUInt,
		eAmf::VectorDouble,
		eAmf::VectorObject,
		eAmf::Dictionary
	};
	// Run unimplemented tests to check that errors are thrown if
	// unimplemented AMF values are attempted to be parsed.
	std::ifstream fileStream;
	fileStream.open("AMFBitStreamUnimplementedTest.bin", std::ios::binary);

	// Read a test BitStream from a file
	std::vector<char> baseBitStream;
	char byte = 0;
	while (fileStream.get(byte)) {
		baseBitStream.push_back(byte);
	}

	fileStream.close();

	for (auto value : unimplementedValues) {
		RakNet::BitStream testBitStream;
		for (auto element : baseBitStream) {
			testBitStream.Write(element);
		}
		testBitStream.Write(value);
		bool caughtException = false;
		try {
			ReadFromBitStream(&testBitStream);
		} catch (eAmf unimplementedValueType) {
			caughtException = true;
		}

		ASSERT_EQ(caughtException, true);
	}
}

/**
 * @brief Test reading a packet capture from live from a BitStream
 */
TEST(dCommonTests, AMFDeserializeLivePacketTest) {
	std::ifstream testFileStream;
	testFileStream.open("AMFBitStreamTest.bin", std::ios::binary);

	// Read a test BitStream from a file
	RakNet::BitStream testBitStream;
	char byte = 0;
	while (testFileStream.get(byte)) {
		testBitStream.Write<char>(byte);
	}

	testFileStream.close();

	std::unique_ptr<AMFBaseValue> resultFromFn(ReadFromBitStream(&testBitStream));
	auto result = static_cast<AMFArrayValue*>(resultFromFn.get());
	// Test the outermost array

	ASSERT_EQ(result->Get<std::string>("BehaviorID")->GetValue(), "10447");
	ASSERT_EQ(result->Get<std::string>("objectID")->GetValue(), "288300744895913279");

	// Test the execution state array
	auto executionState = result->GetArray("executionState");

	ASSERT_NE(executionState, nullptr);

	auto strips = executionState->GetArray("strips")->GetDense();

	ASSERT_EQ(strips.size(), 1);

	auto stripsPosition0 = dynamic_cast<AMFArrayValue*>(strips[0]);

	auto actionIndex = stripsPosition0->Get<double>("actionIndex");

	ASSERT_EQ(actionIndex->GetValue(), 0.0f);

	auto stripIdExecution = stripsPosition0->Get<double>("id");

	ASSERT_EQ(stripIdExecution->GetValue(), 0.0f);

	auto stateIdExecution = executionState->Get<double>("stateID");

	ASSERT_EQ(stateIdExecution->GetValue(), 0.0f);

	auto states = result->GetArray("states")->GetDense();

	ASSERT_EQ(states.size(), 1);

	auto firstState = dynamic_cast<AMFArrayValue*>(states[0]);

	auto stateID = firstState->Get<double>("id");

	ASSERT_EQ(stateID->GetValue(), 0.0f);

	auto stripsInState = firstState->GetArray("strips")->GetDense();

	ASSERT_EQ(stripsInState.size(), 1);

	auto firstStrip = dynamic_cast<AMFArrayValue*>(stripsInState[0]);

	auto actionsInFirstStrip = firstStrip->GetArray("actions")->GetDense();

	ASSERT_EQ(actionsInFirstStrip.size(), 3);

	auto actionID = firstStrip->Get<double>("id");

	ASSERT_EQ(actionID->GetValue(), 0.0f);

	auto uiArray = firstStrip->GetArray("ui");

	auto xPos = uiArray->Get<double>("x");
	auto yPos = uiArray->Get<double>("y");

	ASSERT_EQ(xPos->GetValue(), 103.0f);
	ASSERT_EQ(yPos->GetValue(), 82.0f);

	auto stripId = firstStrip->Get<double>("id");

	ASSERT_EQ(stripId->GetValue(), 0.0f);

	auto firstAction = dynamic_cast<AMFArrayValue*>(actionsInFirstStrip[0]);

	auto firstType = firstAction->Get<std::string>("Type");

	ASSERT_EQ(firstType->GetValue(), "OnInteract");

	auto firstCallback = firstAction->Get<std::string>("__callbackID__");

	ASSERT_EQ(firstCallback->GetValue(), "");

	auto secondAction = dynamic_cast<AMFArrayValue*>(actionsInFirstStrip[1]);

	auto secondType = secondAction->Get<std::string>("Type");

	ASSERT_EQ(secondType->GetValue(), "FlyUp");

	auto secondCallback = secondAction->Get<std::string>("__callbackID__");

	ASSERT_EQ(secondCallback->GetValue(), "");

	auto secondDistance = secondAction->Get<double>("Distance");

	ASSERT_EQ(secondDistance->GetValue(), 25.0f);

	auto thirdAction = dynamic_cast<AMFArrayValue*>(actionsInFirstStrip[2]);

	auto thirdType = thirdAction->Get<std::string>("Type");

	ASSERT_EQ(thirdType->GetValue(), "FlyDown");

	auto thirdCallback = thirdAction->Get<std::string>("__callbackID__");

	ASSERT_EQ(thirdCallback->GetValue(), "");

	auto thirdDistance = thirdAction->Get<double>("Distance");

	ASSERT_EQ(thirdDistance->GetValue(), 25.0f);
}

/**
 * @brief Tests that having no BitStream returns a nullptr.
 */
TEST(dCommonTests, AMFDeserializeNullTest) {
	std::unique_ptr<AMFBaseValue> result(ReadFromBitStream(nullptr));
	ASSERT_EQ(result.get(), nullptr);
}

TEST(dCommonTests, AMFBadConversionTest) {
	std::ifstream testFileStream;
	testFileStream.open("AMFBitStreamTest.bin", std::ios::binary);

	// Read a test BitStream from a file
	RakNet::BitStream testBitStream;
	char byte = 0;
	while (testFileStream.get(byte)) {
		testBitStream.Write<char>(byte);
	}

	testFileStream.close();

	std::unique_ptr<AMFBaseValue> resultFromFn(ReadFromBitStream(&testBitStream));
	auto result = static_cast<AMFArrayValue*>(resultFromFn.get());

	// Actually a string value.
	ASSERT_EQ(result->Get<double>("BehaviorID"), nullptr);

	// Does not exist in the associative portion
	ASSERT_EQ(result->Get<nullptr_t>("DOES_NOT_EXIST"), nullptr);

	result->Push(true);

	// Exists and is correct type
	ASSERT_NE(result->Get<bool>(0), nullptr);

	// Value exists but is wrong typing
	ASSERT_EQ(result->Get<std::string>(0), nullptr);

	// Value is out of bounds
	ASSERT_EQ(result->Get<bool>(1), nullptr);
}

/**
 * Below is the AMF that is in the AMFBitStreamTest.bin file that we are reading in
 * from a bitstream to test.
args: amf3!
{
	"objectID": "288300744895913279",
	"BehaviorID": "10447",
	"executionState": amf3!
	{
		"strips": amf3!
		[
			amf3!
			{
				"actionIndex": 0.0,
				"id": 0.0,
			},
		],
		"stateID": 0.0,
	},
	"states": amf3!
	[
		amf3!
		{
			"id": 0.0,
			"strips": amf3!
			[
				amf3!
				{
					"actions": amf3!
					[
						amf3!
						{
							"Type": "OnInteract",
							"__callbackID__": "",
						},
						amf3!
						{
							"Distance": 25.0,
							"Type": "FlyUp",
							"__callbackID__": "",
						},
						amf3!
						{
							"Distance": 25.0,
							"Type": "FlyDown",
							"__callbackID__": "",
						},
					],
					"id": 0.0,
					"ui": amf3!
					{
						"x": 103.0,
						"y": 82.0,
					},
				},
			],
		},
	],
}
 */
