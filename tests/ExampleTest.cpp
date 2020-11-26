#include <gtest/gtest.h>

double timesTwo(double x);

//The only function: simply multiplies a number by 2
double timesTwo(double x){
    return x*2;
}

TEST(testTimesTwo, integerTests){
    EXPECT_EQ(0, timesTwo(0));
    EXPECT_EQ(2, timesTwo(1));
    EXPECT_EQ(246, timesTwo(123));
}

int main(int argc, char* argv[]){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}