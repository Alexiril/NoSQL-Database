#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "socketCallsMock.hpp"

#include "../ApplicationServer.hpp"
#include "../ApplicationServer.cpp"

using ::testing::Return;
using ::testing::Throw;
using ::testing::_;
using ::testing::ElementsAre;
