#include <wss/cancellation.hpp>
#include <wss/task.hpp>

#include "test_framework.hpp"

WSS_TEST(Task, StartsPending) {
    wss::FunctionTask t([] {});
    EXPECT_EQ(t.state(), wss::TaskState::Pending);
    EXPECT_TRUE(t.id() != 0);
}

WSS_TEST(Task, RunCompletes) {
    bool ran = false;
    wss::FunctionTask t([&] { ran = true; });
    t.execute();
    EXPECT_TRUE(ran);
    EXPECT_EQ(t.state(), wss::TaskState::Completed);
}

WSS_TEST(Task, CancelBeforeRunSkipsBody) {
    bool ran = false;
    wss::FunctionTask t([&] { ran = true; });
    EXPECT_TRUE(t.cancel());
    t.execute();
    EXPECT_FALSE(ran);
    EXPECT_EQ(t.state(), wss::TaskState::Cancelled);
}

WSS_TEST(Task, CancelAfterRunFails) {
    wss::FunctionTask t([] {});
    t.execute();
    EXPECT_FALSE(t.cancel());
    EXPECT_EQ(t.state(), wss::TaskState::Completed);
}

WSS_TEST(Task, ThrowingBodyFails) {
    wss::FunctionTask t([] { throw std::runtime_error("boom"); });
    t.execute();
    EXPECT_EQ(t.state(), wss::TaskState::Failed);
}

WSS_TEST(Task, AlreadyCancelledTokenNeverRuns) {
    wss::CancellationSource src;
    src.cancel();
    bool ran = false;
    wss::FunctionTask t([&] { ran = true; }, src.token());
    t.execute();
    EXPECT_FALSE(ran);
    EXPECT_EQ(t.state(), wss::TaskState::Cancelled);
}

WSS_TEST(Task, IdsAreUnique) {
    wss::FunctionTask a([] {});
    wss::FunctionTask b([] {});
    EXPECT_NE(a.id(), b.id());
}

WSS_TEST(Task, Kinds) {
    wss::FunctionTask f([] {});
    wss::ContinuationTask c([] {});
    wss::TimerTask t([] {});
    EXPECT_TRUE(std::string(f.kind()) == "function");
    EXPECT_TRUE(std::string(c.kind()) == "continuation");
    EXPECT_TRUE(std::string(t.kind()) == "timer");
}

WSS_TEST_MAIN()
