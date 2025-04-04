//============================================================================
//  The contents of this file are covered by the Viskores license. See
//  LICENSE.txt for details.
//
//  By contributing to this file, all contributors agree to the Developer
//  Certificate of Origin Version 1.1 (DCO 1.1) as stated in DCO.txt.
//============================================================================

//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#ifndef viskores_benchmarking_Benchmarker_h
#define viskores_benchmarking_Benchmarker_h
#include <sstream>
#include <viskores/cont/Initialize.h>
#include <viskores/cont/Logging.h>
#include <viskores/cont/RuntimeDeviceTracker.h>
#include <viskores/cont/Timer.h>

#include <viskores/List.h>

#include <viskores/internal/Meta.h>

#include <benchmark/benchmark.h>

#include <ostream>

/// \file Benchmarker.h
/// \brief Benchmarking utilities
///
/// Viskores's benchmarking framework is built on top of Google Benchmark.
///
/// A benchmark is now a single function, which is passed to a macro:
///
/// ```
/// void MyBenchmark(::benchmark::State& state)
/// {
///   MyClass someClass;
///
///   // Optional: Add a descriptive label with additional benchmark details:
///   state.SetLabel("Blah blah blah.");
///
///   // Must use a viskores timer to properly capture eg. CUDA execution times.
///   viskores::cont::Timer timer;
///   for (auto _ : state)
///   {
///     someClass.Reset();
///
///     timer.Start();
///     someClass.DoWork();
///     timer.Stop();
///
///     state.SetIterationTime(timer.GetElapsedTime());
///   }
///
///   // Optional: Report items and/or bytes processed per iteration in output:
///   state.SetItemsProcessed(state.iterations() * someClass.GetNumberOfItems());
///   state.SetBytesProcessed(state.iterations() * someClass.GetNumberOfBytes());
/// }
/// }
/// VISKORES_BENCHMARK(MyBenchmark);
/// ```
///
/// Google benchmark also makes it easy to implement parameter sweep benchmarks:
///
/// ```
/// void MyParameterSweep(::benchmark::State& state)
/// {
///   // The current value in the sweep:
///   const viskores::Id currentValue = state.range(0);
///
///   MyClass someClass;
///   someClass.SetSomeParameter(currentValue);
///
///   viskores::cont::Timer timer;
///   for (auto _ : state)
///   {
///     someClass.Reset();
///
///     timer.Start();
///     someClass.DoWork();
///     timer.Stop();
///
///     state.SetIterationTime(timer.GetElapsedTime());
///   }
/// }
/// VISKORES_BENCHMARK_OPTS(MyBenchmark, ->ArgName("Param")->Range(32, 1024 * 1024));
/// ```
///
/// will generate and launch several benchmarks, exploring the parameter space of
/// `SetSomeParameter` between the values of 32 and (1024*1024). The chain of
///   functions calls in the second argument is applied to an instance of
/// ::benchmark::internal::Benchmark. See Google Benchmark's documentation for
/// more details.
///
/// For more complex benchmark configurations, the VISKORES_BENCHMARK_APPLY macro
///   accepts a function with the signature
/// `void Func(::benchmark::internal::Benchmark*)` that may be used to generate
/// more complex configurations.
///
/// To instantiate a templated benchmark across a list of types, the
/// VISKORES_BENCHMARK_TEMPLATE* macros take a viskores::List of types as an additional
/// parameter. The templated benchmark function will be instantiated and called
/// for each type in the list:
///
/// ```
/// template <typename T>
/// void MyBenchmark(::benchmark::State& state)
/// {
///   MyClass<T> someClass;
///
///   // Must use a viskores timer to properly capture eg. CUDA execution times.
///   viskores::cont::Timer timer;
///   for (auto _ : state)
///   {
///     someClass.Reset();
///
///     timer.Start();
///     someClass.DoWork();
///     timer.Stop();
///
///     state.SetIterationTime(timer.GetElapsedTime());
///   }
/// }
/// }
/// VISKORES_BENCHMARK_TEMPLATE(MyBenchmark, viskores::List<viskores::Float32, viskores::Vec3f_32>);
/// ```
///
/// The benchmarks are executed by calling the `VISKORES_EXECUTE_BENCHMARKS(argc, argv)`
/// macro from `main`. There is also a `VISKORES_EXECUTE_BENCHMARKS_PREAMBLE(argc, argv, some_string)`
/// macro that appends the contents of `some_string` to the Google Benchmark preamble.
///
/// If a benchmark is not compatible with some configuration, it may call
/// `state.SkipWithError("Error message");` on the `::benchmark::State` object and return. This is
/// useful, for instance in the filter tests when the input is not compatible with the filter.
///
/// When launching a benchmark executable, the following options are supported by Google Benchmark:
///
/// - `--benchmark_list_tests`: List all available tests.
/// - `--benchmark_filter="[regex]"`: Only run benchmark with names that match `[regex]`.
/// - `--benchmark_filter="-[regex]"`: Only run benchmark with names that DON'T match `[regex]`.
/// - `--benchmark_min_time=[float]`: Make sure each benchmark repetition gathers `[float]` seconds
///   of data.
/// - `--benchmark_repetitions=[int]`: Run each benchmark `[int]` times and report aggregate statistics
///   (mean, stdev, etc). A "repetition" refers to a single execution of the benchmark function, not
///   an "iteration", which is a loop of the `for(auto _:state){...}` section.
/// - `--benchmark_report_aggregates_only="true|false"`: If true, only the aggregate statistics are
///   reported (affects both console and file output). Requires `--benchmark_repetitions` to be useful.
/// - `--benchmark_display_aggregates_only="true|false"`: If true, only the aggregate statistics are
///   printed to the terminal. Any file output will still contain all repetition info.
/// - `--benchmark_format="console|json|csv"`: Specify terminal output format: human readable
///   (`console`) or `csv`/`json` formats.
/// - `--benchmark_out_format="console|json|csv"`: Specify file output format: human readable
///   (`console`) or `csv`/`json` formats.
/// - `--benchmark_out=[filename]`: Specify output file.
/// - `--benchmark_color="true|false"`: Toggle color output in terminal when using `console` output.
/// - `--benchmark_counters_tabular="true|false"`: Print counter information (e.g. bytes/sec, items/sec)
///   in the table, rather than appending them as a label.
///
/// For more information and examples of practical usage, take a look at the existing benchmarks in
/// viskores/benchmarking/.

/// \def VISKORES_EXECUTE_BENCHMARKS(argc, argv)
///
/// Run the benchmarks defined in the current file. Benchmarks may be filtered
/// and modified using the passed arguments; see the Google Benchmark documentation
/// for more details.
#define VISKORES_EXECUTE_BENCHMARKS(argc, argv) \
  viskores::bench::detail::ExecuteBenchmarks(argc, argv)

/// \def VISKORES_EXECUTE_BENCHMARKS_PREAMBLE(argc, argv, preamble)
///
/// Run the benchmarks defined in the current file. Benchmarks may be filtered
/// and modified using the passed arguments; see the Google Benchmark documentation
/// for more details. The `preamble` string may be used to supply additional
/// information that will be appended to the output's preamble.
#define VISKORES_EXECUTE_BENCHMARKS_PREAMBLE(argc, argv, preamble) \
  viskores::bench::detail::ExecuteBenchmarks(argc, argv, preamble)

/// \def VISKORES_BENCHMARK(BenchFunc)
///
/// Define a simple benchmark. A single benchmark will be generated that executes
/// `BenchFunc`. `BenchFunc` must have the signature:
///
/// ```
/// void BenchFunc(::benchmark::State& state)
/// ```
#define VISKORES_BENCHMARK(BenchFunc) \
  BENCHMARK(BenchFunc)->UseManualTime()->Unit(benchmark::kMillisecond)

/// \def VISKORES_BENCHMARK_OPTS(BenchFunc, Args)
///
/// Similar to `VISKORES_BENCHMARK`, but allows additional options to be specified
/// on the `::benchmark::internal::Benchmark` object. Example usage:
///
/// ```
/// VISKORES_BENCHMARK_OPTS(MyBenchmark, ->ArgName("MyParam")->Range(32, 1024*1024));
/// ```
///
/// Note the similarity to the raw Google Benchmark usage of
/// `BENCHMARK(MyBenchmark)->ArgName("MyParam")->Range(32, 1024*1024);`. See
/// the Google Benchmark documentation for more details on the available options.
#define VISKORES_BENCHMARK_OPTS(BenchFunc, options) \
  BENCHMARK(BenchFunc)->UseManualTime()->Unit(benchmark::kMillisecond) options

/// \def VISKORES_BENCHMARK_APPLY(BenchFunc, ConfigFunc)
///
/// Similar to `VISKORES_BENCHMARK`, but allows advanced benchmark configuration
/// via a supplied ConfigFunc, similar to Google Benchmark's
/// `BENCHMARK(BenchFunc)->Apply(ConfigFunc)`. `ConfigFunc` must have the
/// signature:
///
/// ```
/// void ConfigFunc(::benchmark::internal::Benchmark*);
/// ```
///
/// See the Google Benchmark documentation for more details on the available options.
#define VISKORES_BENCHMARK_APPLY(BenchFunc, applyFunctor) \
  BENCHMARK(BenchFunc)->Apply(applyFunctor)->UseManualTime()->Unit(benchmark::kMillisecond)

/// \def VISKORES_BENCHMARK_TEMPLATES(BenchFunc, TypeList)
///
/// Define a family of benchmark that vary by template argument. A single
/// benchmark will be generated for each type in `TypeList` (a viskores::List of
/// types) that executes `BenchFunc<T>`. `BenchFunc` must have the signature:
///
/// ```
/// template <typename T>
/// void BenchFunc(::benchmark::State& state)
/// ```
#define VISKORES_BENCHMARK_TEMPLATES(BenchFunc, TypeList) \
  VISKORES_BENCHMARK_TEMPLATES_APPLY(BenchFunc, viskores::bench::detail::NullApply, TypeList)

/// \def VISKORES_BENCHMARK_TEMPLATES_OPTS(BenchFunc, Args, TypeList)
///
/// Similar to `VISKORES_BENCHMARK_TEMPLATES`, but allows additional options to be specified
/// on the `::benchmark::internal::Benchmark` object. Example usage:
///
/// ```
/// VISKORES_BENCHMARK_TEMPLATES_OPTS(MyBenchmark,
///                                ->ArgName("MyParam")->Range(32, 1024*1024),
///                              viskores::List<viskores::Float32, viskores::Vec3f_32>);
/// ```
#define VISKORES_BENCHMARK_TEMPLATES_OPTS(BenchFunc, options, TypeList)                      \
  VISKORES_BENCHMARK_TEMPLATES_APPLY(                                                        \
    BenchFunc,                                                                               \
    [](::benchmark::internal::Benchmark* bm) { bm options->Unit(benchmark::kMillisecond); }, \
    TypeList)

/// \def VISKORES_BENCHMARK_TEMPLATES_APPLY(BenchFunc, ConfigFunc, TypeList)
///
/// Similar to `VISKORES_BENCHMARK_TEMPLATES`, but allows advanced benchmark configuration
/// via a supplied ConfigFunc, similar to Google Benchmark's
/// `BENCHMARK(BenchFunc)->Apply(ConfigFunc)`. `ConfigFunc` must have the
/// signature:
///
/// ```
/// void ConfigFunc(::benchmark::internal::Benchmark*);
/// ```
///
/// See the Google Benchmark documentation for more details on the available options.
#define VISKORES_BENCHMARK_TEMPLATES_APPLY(BenchFunc, ApplyFunctor, TypeList)                        \
  namespace                                                                                          \
  { /* A template function cannot be used as a template parameter, so wrap the function with       \
     * a template struct to get it into the GenerateTemplateBenchmarks class. */ \
  template <typename... Ts>                                                                          \
  struct VISKORES_BENCHMARK_WRAPPER_NAME(BenchFunc)                                                  \
  {                                                                                                  \
    static ::benchmark::internal::Function* GetFunction()                                            \
    {                                                                                                \
      return BenchFunc<Ts...>;                                                                       \
    }                                                                                                \
  };                                                                                                 \
  } /* end anon namespace */                                                                         \
  int BENCHMARK_PRIVATE_NAME(BenchFunc) = viskores::bench::detail::                                  \
    GenerateTemplateBenchmarks<VISKORES_BENCHMARK_WRAPPER_NAME(BenchFunc), TypeList>::Register(      \
      #BenchFunc, ApplyFunctor)

// Internal use only:
#define VISKORES_BENCHMARK_WRAPPER_NAME(BenchFunc) \
  BENCHMARK_PRIVATE_CONCAT(_wrapper_, BenchFunc, __LINE__)

namespace viskores
{
namespace bench
{
namespace detail
{

static inline void NullApply(::benchmark::internal::Benchmark*) {}

/// Do not use directly. The VISKORES_BENCHMARK_TEMPLATES macros should be used
/// instead.
// TypeLists could be expanded to compute cross products if we ever have that
// need.
template <template <typename...> class BenchType, typename TypeList>
struct GenerateTemplateBenchmarks
{
private:
  template <typename T>
  using MakeBenchType = BenchType<T>;

  using Benchmarks = viskores::ListTransform<TypeList, MakeBenchType>;

  template <typename ApplyFunctor>
  struct RegisterImpl
  {
    std::string BenchName;
    ApplyFunctor Apply;

    template <typename P>
    void operator()(viskores::internal::meta::Type<BenchType<P>>) const
    {
      std::ostringstream name;
      name << this->BenchName << "<" << viskores::cont::TypeToString<P>() << ">";
      auto bm = ::benchmark::internal::RegisterBenchmarkInternal(
        new ::benchmark::internal::FunctionBenchmark(name.str().c_str(),
                                                     BenchType<P>::GetFunction()));
      this->Apply(bm);

      // Always use manual time with viskores::cont::Timer to capture CUDA times accurately.
      bm->UseManualTime()->Unit(benchmark::kMillisecond);
    }
  };

public:
  template <typename ApplyFunctor>
  static int Register(const std::string& benchName, ApplyFunctor&& apply)
  {
    viskores::ListForEach(
      RegisterImpl<ApplyFunctor>{ benchName, std::forward<ApplyFunctor>(apply) },
      viskores::ListTransform<Benchmarks, viskores::internal::meta::Type>{});
    return 0;
  }
};

class ViskoresConsoleReporter : public ::benchmark::ConsoleReporter
{
  std::string UserPreamble;

public:
  ViskoresConsoleReporter() = default;

  explicit ViskoresConsoleReporter(const std::string& preamble)
    : UserPreamble{ preamble }
  {
  }

  bool ReportContext(const Context& context) override
  {
    if (!::benchmark::ConsoleReporter::ReportContext(context))
    {
      return false;
    }

    // The rest of the preamble is printed to the error stream, so be consistent:
    auto& out = this->GetErrorStream();

    // Print list of devices:
    out << "Viskores Device State:\n";
    viskores::cont::GetRuntimeDeviceTracker().PrintSummary(out);
    if (!this->UserPreamble.empty())
    {
      out << this->UserPreamble << "\n";
    }
    out.flush();

    return true;
  }
};

// Returns the number of executed benchmarks:
static inline viskores::Id ExecuteBenchmarks(int& argc,
                                             char* argv[],
                                             const std::string& preamble = std::string{})
{
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
  {
    return 1;
  }

  ViskoresConsoleReporter reporter{ preamble };

  viskores::cont::Timer timer;
  timer.Start();
  std::size_t num = ::benchmark::RunSpecifiedBenchmarks(&reporter);
  timer.Stop();

  reporter.GetOutputStream().flush();
  reporter.GetErrorStream().flush();

  reporter.GetErrorStream() << "Ran " << num << " benchmarks in " << timer.GetElapsedTime()
                            << " seconds." << std::endl;

  return static_cast<viskores::Id>(num);
}

void InitializeArgs(int* argc, std::vector<char*>& args, viskores::cont::InitializeOptions& opts)
{
  bool isHelp = false;

  // Inject --help
  if (*argc == 1)
  {
    const char* help = "--help"; // We want it to be static
    args.push_back(const_cast<char*>(help));
    *argc = *argc + 1;
  }

  args.push_back(nullptr);

  for (size_t i = 0; i < static_cast<size_t>(*argc); ++i)
  {
    auto opt_s = std::string(args[i]);
    if (opt_s == "--help" || opt_s == "-help" || opt_s == "-h")
    {
      isHelp = true;
    }
  }

  if (!isHelp)
  {
    return;
  }

  opts = viskores::cont::InitializeOptions::None;
}
}
}
} // end namespace viskores::bench::detail

#endif
