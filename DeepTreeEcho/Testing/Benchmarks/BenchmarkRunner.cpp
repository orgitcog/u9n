//=============================================================================
// BenchmarkRunner.cpp
// 
// Command-line benchmark runner for Deep Tree Echo performance benchmarks.
//
// Usage:
//   RunBenchmarks                    # Run all benchmarks
//   RunBenchmarks --category reservoir  # Run specific category
//   RunBenchmarks --json             # Output JSON format
//   RunBenchmarks --csv              # Output CSV format
//   RunBenchmarks --verbose          # Verbose output
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "PerformanceBenchmark.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

using namespace DeepTreeEcho::Benchmarks;

void PrintUsage(const char* programName)
{
    std::cout << "Deep Tree Echo Performance Benchmark Runner\n";
    std::cout << "============================================\n\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h        Show this help message\n";
    std::cout << "  --all             Run all benchmarks (default)\n";
    std::cout << "  --category <cat>  Run benchmarks for specific category:\n";
    std::cout << "                      reservoir, cognitive, memory, embodied,\n";
    std::cout << "                      temporal, inference, integration\n";
    std::cout << "  --json            Output results in JSON format\n";
    std::cout << "  --csv             Output results in CSV format\n";
    std::cout << "  --markdown, -m    Output results in Markdown format (default)\n";
    std::cout << "  --output, -o <f>  Write results to file instead of stdout\n";
    std::cout << "  --iterations <n>  Number of measurement iterations (default: 100)\n";
    std::cout << "  --warmup <n>      Number of warmup iterations (default: 10)\n";
    std::cout << "  --verbose, -v     Verbose output\n";
    std::cout << "  --samples         Include individual samples in output\n";
    std::cout << "\n";
}

ComponentCategory ParseCategory(const std::string& categoryStr)
{
    if (categoryStr == "reservoir") return ComponentCategory::Reservoir;
    if (categoryStr == "cognitive") return ComponentCategory::CognitiveLoop;
    if (categoryStr == "memory") return ComponentCategory::Memory;
    if (categoryStr == "embodied") return ComponentCategory::Embodied;
    if (categoryStr == "temporal") return ComponentCategory::Temporal;
    if (categoryStr == "inference") return ComponentCategory::ActiveInference;
    if (categoryStr == "integration") return ComponentCategory::Integration;
    return ComponentCategory::All;
}

int main(int argc, char* argv[])
{
    // Default settings
    ComponentCategory category = ComponentCategory::All;
    enum class OutputFormat { Markdown, Json, Csv } outputFormat = OutputFormat::Markdown;
    std::string outputFile;
    bool verbose = false;
    bool collectSamples = false;
    int iterations = 100;
    int warmup = 10;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "--all")
        {
            category = ComponentCategory::All;
        }
        else if (arg == "--category" && i + 1 < argc)
        {
            category = ParseCategory(argv[++i]);
        }
        else if (arg == "--json")
        {
            outputFormat = OutputFormat::Json;
        }
        else if (arg == "--csv")
        {
            outputFormat = OutputFormat::Csv;
        }
        else if (arg == "--markdown" || arg == "-m")
        {
            outputFormat = OutputFormat::Markdown;
        }
        else if ((arg == "--output" || arg == "-o") && i + 1 < argc)
        {
            outputFile = argv[++i];
        }
        else if (arg == "--iterations" && i + 1 < argc)
        {
            iterations = std::stoi(argv[++i]);
        }
        else if (arg == "--warmup" && i + 1 < argc)
        {
            warmup = std::stoi(argv[++i]);
        }
        else if (arg == "--verbose" || arg == "-v")
        {
            verbose = true;
        }
        else if (arg == "--samples")
        {
            collectSamples = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }
    
    // Configure benchmark
    BenchmarkConfig config;
    config.MeasureIterations = iterations;
    config.WarmupIterations = warmup;
    config.VerboseOutput = verbose;
    config.CollectSamples = collectSamples;
    
    // Initialize benchmark
    PerformanceBenchmark benchmark;
    benchmark.Initialize(config);
    
    if (verbose)
    {
        std::cout << "Deep Tree Echo Performance Benchmark\n";
        std::cout << "=====================================\n";
        std::cout << "Configuration:\n";
        std::cout << "  Iterations: " << iterations << "\n";
        std::cout << "  Warmup: " << warmup << "\n";
        std::cout << "  Category: ";
        switch (category)
        {
            case ComponentCategory::Reservoir: std::cout << "Reservoir"; break;
            case ComponentCategory::CognitiveLoop: std::cout << "Cognitive Loop"; break;
            case ComponentCategory::Memory: std::cout << "Memory"; break;
            case ComponentCategory::Embodied: std::cout << "Embodied"; break;
            case ComponentCategory::Temporal: std::cout << "Temporal"; break;
            case ComponentCategory::ActiveInference: std::cout << "Active Inference"; break;
            case ComponentCategory::Integration: std::cout << "Integration"; break;
            case ComponentCategory::All: std::cout << "All"; break;
        }
        std::cout << "\n\nRunning benchmarks...\n\n";
    }
    
    // Run benchmarks
    BenchmarkSuite suite = benchmark.RunBenchmarks(category);
    
    // Generate output
    std::string output;
    switch (outputFormat)
    {
        case OutputFormat::Json:
            output = suite.ExportToJson();
            break;
        case OutputFormat::Csv:
            output = suite.ExportToCsv();
            break;
        case OutputFormat::Markdown:
        default:
            output = suite.ExportToMarkdown();
            break;
    }
    
    // Write output
    if (!outputFile.empty())
    {
        std::ofstream file(outputFile);
        if (file.is_open())
        {
            file << output;
            file.close();
            if (verbose)
            {
                std::cout << "Results written to: " << outputFile << "\n";
            }
        }
        else
        {
            std::cerr << "Error: Could not open output file: " << outputFile << "\n";
            return 1;
        }
    }
    else
    {
        std::cout << output;
    }
    
    // Print summary
    if (verbose)
    {
        std::cout << "\nBenchmark Summary:\n";
        std::cout << "  Total Tests: " << suite.TotalTests << "\n";
        std::cout << "  Passed: " << suite.PassedTests << "\n";
        std::cout << "  Failed: " << suite.FailedTests << "\n";
        std::cout << "  Pass Rate: " << suite.GetPassRate() << "%\n";
        std::cout << "  Total Duration: " << suite.TotalDuration << "s\n";
    }
    
    // Cleanup
    benchmark.Shutdown();
    
    // Return non-zero if any tests failed
    return (suite.FailedTests > 0) ? 1 : 0;
}
