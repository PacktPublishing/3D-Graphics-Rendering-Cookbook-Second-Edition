#include <stdio.h>
#include <fstream>
#include <vector>

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

int main()
{
  tf::Taskflow taskflow;

  std::vector<int> items{ 1, 2, 3, 4, 5, 6, 7, 8 };

  auto task = taskflow.for_each_index(0ull, items.size(), 1ull, [&](int i) { printf("%i", items[i]); }).name("for_each_index");

  taskflow.emplace([]() { printf("\nS - Start\n"); }).name("S").precede(task);
  taskflow.emplace([]() { printf("\nT - End\n"); }).name("T").succeed(task);

  {
    std::ofstream os(".cache/taskflow.dot");
    taskflow.dump(os);
  }

  tf::Executor executor;
  executor.run(taskflow).wait();

  return 0;
}
