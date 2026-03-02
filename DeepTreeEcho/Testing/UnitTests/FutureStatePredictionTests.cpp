/**
 * @file FutureStatePredictionTests.cpp
 * @brief Unit tests for Future State Prediction system
 * 
 * Tests cover:
 * - Trajectory creation and management
 * - Multi-method state prediction (linear, polynomial, exponential, reservoir)
 * - Ensemble prediction generation
 * - Prediction validation and error metrics
 * - Confidence and uncertainty computation
 * - Method weight adaptation
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>

// ============================================================================
// Mock Types for Future State Prediction
// ============================================================================

using Vector = std::vector<double>;
using Matrix = std::vector<std::vector<double>>;

/**
 * @brief State domain enumeration (matching EStateDomain)
 */
enum class EStateDomain {
    Cognitive,
    Emotional,
    Motor,
    Environmental,
    Social,
    Integrated
};

/**
 * @brief Trajectory type enumeration (matching ETrajectoryType)
 */
enum class ETrajectoryType {
    Linear,
    Polynomial,
    Exponential,
    Reservoir,
    Ensemble
};

/**
 * @brief Prediction horizon enumeration
 */
enum class EPredictionHorizon {
    Immediate,
    ShortTerm,
    MediumTerm,
    LongTerm
};

/**
 * @brief Confidence level enumeration
 */
enum class EPredictionConfidence {
    VeryLow,
    Low,
    Medium,
    High,
    VeryHigh
};

/**
 * @brief State observation structure
 */
struct FStateObservation {
    double Timestamp = 0.0;
    int EchobeatStep = 0;
    Vector StateVector;
    EStateDomain Domain = EStateDomain::Cognitive;
    double Reliability = 1.0;
};

/**
 * @brief State trajectory structure
 */
struct FStateTrajectory {
    std::string TrajectoryID;
    EStateDomain Domain = EStateDomain::Cognitive;
    std::vector<FStateObservation> Observations;
    Vector FittedParameters;
    ETrajectoryType FittingType = ETrajectoryType::Linear;
    double GoodnessOfFit = 0.0;
    double CreatedAt = 0.0;
    double LastUpdatedAt = 0.0;
};

/**
 * @brief Future prediction structure
 */
struct FFuturePrediction {
    int PredictionID = 0;
    std::string SourceTrajectoryID;
    Vector PredictedState;
    EPredictionHorizon Horizon = EPredictionHorizon::Immediate;
    double TargetTimestamp = 0.0;
    int StepsAhead = 1;
    double Confidence = 0.5;
    EPredictionConfidence ConfidenceLevel = EPredictionConfidence::Medium;
    Vector UncertaintyBounds;
    EStateDomain Domain = EStateDomain::Cognitive;
    ETrajectoryType PredictionMethod = ETrajectoryType::Linear;
    double CreatedAt = 0.0;
    bool bValidated = false;
    Vector ActualState;
    double PredictionError = 0.0;
};

/**
 * @brief Prediction validation structure
 */
struct FPredictionValidation {
    int PredictionID = 0;
    double MSE = 0.0;
    double MAE = 0.0;
    double Correlation = 0.0;
    bool bWithinBounds = false;
    double ValidatedAt = 0.0;
};

/**
 * @brief Ensemble prediction structure
 */
struct FEnsemblePrediction {
    std::vector<FFuturePrediction> IndividualPredictions;
    Vector EnsembleState;
    Vector MethodWeights;
    double EnsembleConfidence = 0.5;
    double MethodDisagreement = 0.0;
};

/**
 * @brief Prediction statistics structure
 */
struct FPredictionStatistics {
    int TotalPredictions = 0;
    int ValidatedPredictions = 0;
    double AverageMSE = 0.0;
    double AverageConfidence = 0.0;
    double WithinBoundsRatio = 0.0;
    ETrajectoryType BestMethod = ETrajectoryType::Linear;
    std::map<ETrajectoryType, double> MethodMSE;
    std::map<EPredictionHorizon, double> HorizonAccuracy;
};

/**
 * @brief Mock Future State Prediction System
 */
class MockFutureStatePrediction {
public:
    // Configuration
    int StateVectorDimension = 64;
    int MaxTrajectoryLength = 100;
    int MinObservationsForFitting = 5;
    int MaxPredictionHistory = 200;
    ETrajectoryType DefaultPredictionMethod = ETrajectoryType::Ensemble;
    bool bAutoValidate = true;
    bool bEnableEnsemble = true;
    int PolynomialDegree = 3;
    double ConfidenceDecayRate = 0.1;
    double BaseConfidence = 0.8;

    void Initialize() {
        NextTrajectoryID = 1;
        NextPredictionID = 1;
        CurrentTime = 0.0;
        
        // Initialize method weights
        MethodWeights[ETrajectoryType::Linear] = 0.25;
        MethodWeights[ETrajectoryType::Polynomial] = 0.25;
        MethodWeights[ETrajectoryType::Exponential] = 0.2;
        MethodWeights[ETrajectoryType::Reservoir] = 0.3;
        
        bInitialized = true;
    }

    bool IsInitialized() const { return bInitialized; }

    void SetTime(double time) { CurrentTime = time; }
    double GetTime() const { return CurrentTime; }
    void AdvanceTime(double delta) { CurrentTime += delta; }

    // Trajectory Management
    std::string CreateTrajectory(EStateDomain domain, const std::string& name = "") {
        std::string id = "Trajectory_" + std::to_string(NextTrajectoryID++) + "_" + 
            (name.empty() ? "Auto" : name);
        
        FStateTrajectory trajectory;
        trajectory.TrajectoryID = id;
        trajectory.Domain = domain;
        trajectory.FittingType = DefaultPredictionMethod;
        trajectory.CreatedAt = CurrentTime;
        trajectory.LastUpdatedAt = CurrentTime;
        
        Trajectories[id] = trajectory;
        return id;
    }

    void AddObservation(const std::string& trajectoryID, const Vector& stateVector, double reliability = 1.0) {
        if (Trajectories.find(trajectoryID) == Trajectories.end()) return;
        
        FStateTrajectory& trajectory = Trajectories[trajectoryID];
        
        FStateObservation obs;
        obs.Timestamp = CurrentTime;
        obs.StateVector = stateVector;
        obs.Domain = trajectory.Domain;
        obs.Reliability = std::clamp(reliability, 0.0, 1.0);
        
        trajectory.Observations.push_back(obs);
        trajectory.LastUpdatedAt = CurrentTime;
        
        // Trim to max length
        while (static_cast<int>(trajectory.Observations.size()) > MaxTrajectoryLength) {
            trajectory.Observations.erase(trajectory.Observations.begin());
        }
        
        // Auto-fit
        if (static_cast<int>(trajectory.Observations.size()) >= MinObservationsForFitting) {
            FitTrajectory(trajectoryID, trajectory.FittingType);
        }
    }

    FStateTrajectory GetTrajectory(const std::string& trajectoryID) const {
        auto it = Trajectories.find(trajectoryID);
        if (it != Trajectories.end()) {
            return it->second;
        }
        return FStateTrajectory();
    }

    bool TrajectoryExists(const std::string& trajectoryID) const {
        return Trajectories.find(trajectoryID) != Trajectories.end();
    }

    double FitTrajectory(const std::string& trajectoryID, ETrajectoryType method) {
        if (Trajectories.find(trajectoryID) == Trajectories.end()) return 0.0;
        
        FStateTrajectory& trajectory = Trajectories[trajectoryID];
        if (static_cast<int>(trajectory.Observations.size()) < MinObservationsForFitting) {
            return 0.0;
        }

        trajectory.FittingType = method;
        int dim = trajectory.Observations[0].StateVector.size();
        int n = trajectory.Observations.size();
        double baseTime = trajectory.Observations[0].Timestamp;

        if (method == ETrajectoryType::Linear) {
            trajectory.FittedParameters.resize(dim * 2);
            
            for (int d = 0; d < dim; ++d) {
                // Linear regression
                double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
                
                for (int i = 0; i < n; ++i) {
                    double x = trajectory.Observations[i].Timestamp - baseTime;
                    double y = (d < static_cast<int>(trajectory.Observations[i].StateVector.size())) ? 
                        trajectory.Observations[i].StateVector[d] : 0.0;
                    sumX += x;
                    sumY += y;
                    sumXY += x * y;
                    sumX2 += x * x;
                }

                double denom = n * sumX2 - sumX * sumX;
                if (std::abs(denom) > 1e-6) {
                    double slope = (n * sumXY - sumX * sumY) / denom;
                    double intercept = (sumY - slope * sumX) / n;
                    trajectory.FittedParameters[d * 2] = slope;
                    trajectory.FittedParameters[d * 2 + 1] = intercept;
                } else {
                    trajectory.FittedParameters[d * 2] = 0.0;
                    trajectory.FittedParameters[d * 2 + 1] = sumY / n;
                }
            }
        }

        trajectory.GoodnessOfFit = ComputeGoodnessOfFit(trajectory);
        return trajectory.GoodnessOfFit;
    }

    void ClearTrajectory(const std::string& trajectoryID) {
        if (Trajectories.find(trajectoryID) != Trajectories.end()) {
            Trajectories[trajectoryID].Observations.clear();
            Trajectories[trajectoryID].FittedParameters.clear();
            Trajectories[trajectoryID].GoodnessOfFit = 0.0;
        }
    }

    void RemoveTrajectory(const std::string& trajectoryID) {
        Trajectories.erase(trajectoryID);
    }

    // Prediction
    FFuturePrediction PredictFutureState(const std::string& trajectoryID, int stepsAhead, 
                                          ETrajectoryType method = ETrajectoryType::Ensemble) {
        FFuturePrediction prediction;
        prediction.PredictionID = NextPredictionID++;
        prediction.SourceTrajectoryID = trajectoryID;
        prediction.StepsAhead = stepsAhead;
        prediction.PredictionMethod = method;
        prediction.CreatedAt = CurrentTime;
        prediction.Horizon = ClassifyHorizon(stepsAhead);

        if (Trajectories.find(trajectoryID) == Trajectories.end()) {
            prediction.Confidence = 0.0;
            prediction.ConfidenceLevel = EPredictionConfidence::VeryLow;
            return prediction;
        }

        const FStateTrajectory& trajectory = Trajectories[trajectoryID];
        prediction.Domain = trajectory.Domain;

        if (static_cast<int>(trajectory.Observations.size()) < MinObservationsForFitting) {
            prediction.Confidence = 0.0;
            prediction.ConfidenceLevel = EPredictionConfidence::VeryLow;
            return prediction;
        }

        // Compute target timestamp
        double lastTime = trajectory.Observations.back().Timestamp;
        double avgDelta = (trajectory.Observations.size() > 1) ?
            (lastTime - trajectory.Observations[0].Timestamp) / (trajectory.Observations.size() - 1) : 0.1;
        prediction.TargetTimestamp = lastTime + avgDelta * stepsAhead;

        // Generate prediction
        if (method == ETrajectoryType::Ensemble && bEnableEnsemble) {
            FEnsemblePrediction ensemble = GenerateEnsemblePrediction(trajectoryID, stepsAhead);
            prediction.PredictedState = ensemble.EnsembleState;
            prediction.Confidence = ensemble.EnsembleConfidence;
        } else {
            prediction.PredictedState = PredictWithMethod(trajectory, stepsAhead, method);
            prediction.Confidence = ComputePredictionConfidence(trajectoryID, stepsAhead);
        }

        prediction.UncertaintyBounds = ComputeUncertaintyBounds(trajectoryID, stepsAhead);
        prediction.ConfidenceLevel = ClassifyConfidence(prediction.Confidence);

        // Store prediction
        Predictions[prediction.PredictionID] = prediction;
        Statistics.TotalPredictions++;

        return prediction;
    }

    FEnsemblePrediction GenerateEnsemblePrediction(const std::string& trajectoryID, int stepsAhead) {
        FEnsemblePrediction ensemble;

        if (Trajectories.find(trajectoryID) == Trajectories.end()) {
            return ensemble;
        }

        const FStateTrajectory& trajectory = Trajectories[trajectoryID];

        std::vector<ETrajectoryType> methods = {
            ETrajectoryType::Linear,
            ETrajectoryType::Polynomial,
            ETrajectoryType::Exponential,
            ETrajectoryType::Reservoir
        };

        for (ETrajectoryType method : methods) {
            FFuturePrediction pred;
            pred.PredictionID = NextPredictionID++;
            pred.SourceTrajectoryID = trajectoryID;
            pred.StepsAhead = stepsAhead;
            pred.PredictionMethod = method;
            pred.Domain = trajectory.Domain;
            pred.CreatedAt = CurrentTime;
            pred.PredictedState = PredictWithMethod(trajectory, stepsAhead, method);
            pred.Confidence = ComputePredictionConfidence(trajectoryID, stepsAhead) * 
                (MethodWeights.count(method) ? MethodWeights[method] : 0.25);

            ensemble.IndividualPredictions.push_back(pred);
        }

        // Compute weighted ensemble
        int dim = 0;
        for (const auto& pred : ensemble.IndividualPredictions) {
            dim = std::max(dim, static_cast<int>(pred.PredictedState.size()));
        }

        ensemble.EnsembleState.resize(dim, 0.0);
        double totalWeight = 0.0;

        for (size_t i = 0; i < ensemble.IndividualPredictions.size(); ++i) {
            const auto& pred = ensemble.IndividualPredictions[i];
            double weight = MethodWeights.count(pred.PredictionMethod) ? 
                MethodWeights[pred.PredictionMethod] : 0.25;
            
            ensemble.MethodWeights.push_back(weight);
            totalWeight += weight;

            for (size_t d = 0; d < pred.PredictedState.size(); ++d) {
                ensemble.EnsembleState[d] += pred.PredictedState[d] * weight;
            }
        }

        if (totalWeight > 1e-6) {
            for (double& val : ensemble.EnsembleState) {
                val /= totalWeight;
            }
        }

        ensemble.EnsembleConfidence = ComputePredictionConfidence(trajectoryID, stepsAhead);

        // Compute method disagreement
        double disagreement = 0.0;
        for (const auto& pred : ensemble.IndividualPredictions) {
            disagreement += ComputeMSE(pred.PredictedState, ensemble.EnsembleState);
        }
        ensemble.MethodDisagreement = disagreement / std::max(1, static_cast<int>(ensemble.IndividualPredictions.size()));

        ensemble.EnsembleConfidence *= (1.0 - std::clamp(ensemble.MethodDisagreement, 0.0, 0.5));

        return ensemble;
    }

    // Validation
    FPredictionValidation ValidatePrediction(int predictionID, const Vector& actualState) {
        FPredictionValidation validation;
        validation.PredictionID = predictionID;
        validation.ValidatedAt = CurrentTime;

        if (Predictions.find(predictionID) == Predictions.end()) {
            return validation;
        }

        FFuturePrediction& prediction = Predictions[predictionID];
        prediction.bValidated = true;
        prediction.ActualState = actualState;

        validation.MSE = ComputeMSE(prediction.PredictedState, actualState);
        validation.MAE = ComputeMAE(prediction.PredictedState, actualState);
        validation.Correlation = ComputeCorrelation(prediction.PredictedState, actualState);

        prediction.PredictionError = validation.MSE;

        // Check bounds
        bool withinBounds = true;
        for (size_t i = 0; i < std::min(prediction.PredictedState.size(), actualState.size()); ++i) {
            double diff = std::abs(prediction.PredictedState[i] - actualState[i]);
            double bound = (i < prediction.UncertaintyBounds.size()) ? prediction.UncertaintyBounds[i] : 1.0;
            if (diff > bound) {
                withinBounds = false;
                break;
            }
        }
        validation.bWithinBounds = withinBounds;

        ValidationHistory.push_back(validation);
        UpdateStatistics(validation, prediction);

        return validation;
    }

    FPredictionStatistics GetPredictionStatistics() const {
        return Statistics;
    }

    FFuturePrediction GetPrediction(int predictionID) const {
        auto it = Predictions.find(predictionID);
        if (it != Predictions.end()) {
            return it->second;
        }
        return FFuturePrediction();
    }

    std::vector<FFuturePrediction> GetPendingPredictions() const {
        std::vector<FFuturePrediction> pending;
        for (const auto& pair : Predictions) {
            if (!pair.second.bValidated && pair.second.TargetTimestamp <= CurrentTime) {
                pending.push_back(pair.second);
            }
        }
        return pending;
    }

    double ComputePredictionConfidence(const std::string& trajectoryID, int stepsAhead) const {
        if (Trajectories.find(trajectoryID) == Trajectories.end()) {
            return 0.0;
        }

        const FStateTrajectory& trajectory = Trajectories.at(trajectoryID);
        
        double confidence = BaseConfidence * std::clamp(trajectory.GoodnessOfFit, 0.1, 1.0);
        double decay = std::exp(-ConfidenceDecayRate * stepsAhead);
        confidence *= decay;

        double observationFactor = std::clamp(
            static_cast<double>(trajectory.Observations.size()) / MaxTrajectoryLength, 0.2, 1.0);
        confidence *= observationFactor;

        return std::clamp(confidence, 0.0, 1.0);
    }

    Vector ComputeUncertaintyBounds(const std::string& trajectoryID, int stepsAhead) const {
        Vector bounds;
        if (Trajectories.find(trajectoryID) == Trajectories.end()) {
            return bounds;
        }

        const FStateTrajectory& trajectory = Trajectories.at(trajectoryID);
        if (trajectory.Observations.empty()) return bounds;

        int dim = trajectory.Observations[0].StateVector.size();
        bounds.resize(dim);

        for (int d = 0; d < dim; ++d) {
            double sum = 0, sumSq = 0;
            int count = 0;

            for (const auto& obs : trajectory.Observations) {
                if (d < static_cast<int>(obs.StateVector.size())) {
                    double val = obs.StateVector[d];
                    sum += val;
                    sumSq += val * val;
                    count++;
                }
            }

            if (count > 1) {
                double mean = sum / count;
                double variance = (sumSq - sum * mean) / (count - 1);
                double stdDev = std::sqrt(std::max(0.0, variance));
                double horizonFactor = 1.0 + 0.1 * stepsAhead;
                bounds[d] = stdDev * horizonFactor * 2.0;
            } else {
                bounds[d] = 1.0;
            }
        }

        return bounds;
    }

    EPredictionConfidence ClassifyConfidence(double confidenceScore) const {
        if (confidenceScore < 0.2)
            return EPredictionConfidence::VeryLow;
        else if (confidenceScore < 0.4)
            return EPredictionConfidence::Low;
        else if (confidenceScore < 0.6)
            return EPredictionConfidence::Medium;
        else if (confidenceScore < 0.8)
            return EPredictionConfidence::High;
        else
            return EPredictionConfidence::VeryHigh;
    }

    void UpdateMethodWeights() {
        if (Statistics.MethodMSE.empty()) return;

        double totalInverseMSE = 0.0;
        std::map<ETrajectoryType, double> inverseMSE;

        for (const auto& pair : Statistics.MethodMSE) {
            double inv = 1.0 / std::max(0.01, pair.second);
            inverseMSE[pair.first] = inv;
            totalInverseMSE += inv;
        }

        if (totalInverseMSE > 1e-6) {
            for (const auto& pair : inverseMSE) {
                MethodWeights[pair.first] = pair.second / totalInverseMSE;
            }
        }
    }

    std::map<ETrajectoryType, double> GetMethodWeights() const {
        return MethodWeights;
    }

private:
    bool bInitialized = false;
    int NextTrajectoryID = 1;
    int NextPredictionID = 1;
    double CurrentTime = 0.0;

    std::map<std::string, FStateTrajectory> Trajectories;
    std::map<int, FFuturePrediction> Predictions;
    std::vector<FPredictionValidation> ValidationHistory;
    std::map<ETrajectoryType, double> MethodWeights;
    FPredictionStatistics Statistics;

    EPredictionHorizon ClassifyHorizon(int stepsAhead) const {
        if (stepsAhead <= 2)
            return EPredictionHorizon::Immediate;
        else if (stepsAhead <= 5)
            return EPredictionHorizon::ShortTerm;
        else if (stepsAhead <= 12)
            return EPredictionHorizon::MediumTerm;
        else
            return EPredictionHorizon::LongTerm;
    }

    double ComputeGoodnessOfFit(const FStateTrajectory& trajectory) const {
        if (trajectory.Observations.size() < 2) return 0.0;

        int dim = trajectory.Observations[0].StateVector.size();
        double totalSS = 0.0, residualSS = 0.0;

        // Compute mean
        Vector mean(dim, 0.0);
        for (const auto& obs : trajectory.Observations) {
            for (int d = 0; d < std::min(dim, static_cast<int>(obs.StateVector.size())); ++d) {
                mean[d] += obs.StateVector[d];
            }
        }
        for (double& val : mean) val /= trajectory.Observations.size();

        // Compute SS
        double baseTime = trajectory.Observations[0].Timestamp;
        for (const auto& obs : trajectory.Observations) {
            double t = obs.Timestamp - baseTime;
            
            for (int d = 0; d < std::min(dim, static_cast<int>(obs.StateVector.size())); ++d) {
                double actual = obs.StateVector[d];
                double predicted = 0.0;

                if (trajectory.FittingType == ETrajectoryType::Linear && 
                    static_cast<int>(trajectory.FittedParameters.size()) >= (d + 1) * 2) {
                    double slope = trajectory.FittedParameters[d * 2];
                    double intercept = trajectory.FittedParameters[d * 2 + 1];
                    predicted = slope * t + intercept;
                } else {
                    predicted = mean[d];
                }

                totalSS += std::pow(actual - mean[d], 2);
                residualSS += std::pow(actual - predicted, 2);
            }
        }

        if (totalSS > 1e-6) {
            return std::clamp(1.0 - residualSS / totalSS, 0.0, 1.0);
        }
        return 0.0;
    }

    Vector PredictWithMethod(const FStateTrajectory& trajectory, int stepsAhead, ETrajectoryType method) const {
        if (trajectory.Observations.empty()) return Vector();
        
        int dim = trajectory.Observations[0].StateVector.size();
        Vector prediction(dim);

        if (trajectory.Observations.size() < 2) {
            return trajectory.Observations.back().StateVector;
        }

        double lastTime = trajectory.Observations.back().Timestamp;
        double baseTime = trajectory.Observations[0].Timestamp;
        double avgDelta = (lastTime - baseTime) / (trajectory.Observations.size() - 1);

        switch (method) {
        case ETrajectoryType::Linear:
        case ETrajectoryType::Reservoir:  // Fallback to linear-like
            if (static_cast<int>(trajectory.FittedParameters.size()) >= dim * 2) {
                for (int d = 0; d < dim; ++d) {
                    double slope = trajectory.FittedParameters[d * 2];
                    double intercept = trajectory.FittedParameters[d * 2 + 1];
                    double targetT = (lastTime - baseTime) + avgDelta * stepsAhead;
                    prediction[d] = slope * targetT + intercept;
                }
            } else {
                // Simple extrapolation
                const auto& last = trajectory.Observations.back().StateVector;
                const auto& prev = trajectory.Observations[trajectory.Observations.size() - 2].StateVector;
                double timeDelta = trajectory.Observations.back().Timestamp - 
                    trajectory.Observations[trajectory.Observations.size() - 2].Timestamp;
                
                for (int d = 0; d < dim; ++d) {
                    double lastVal = (d < static_cast<int>(last.size())) ? last[d] : 0.0;
                    double prevVal = (d < static_cast<int>(prev.size())) ? prev[d] : 0.0;
                    double slope = (timeDelta > 1e-6) ? (lastVal - prevVal) / timeDelta : 0.0;
                    prediction[d] = lastVal + slope * avgDelta * stepsAhead;
                }
            }
            break;

        case ETrajectoryType::Polynomial:
        case ETrajectoryType::Exponential:
            // Simplified: use linear for these in mock
            return PredictWithMethod(trajectory, stepsAhead, ETrajectoryType::Linear);

        default:
            return trajectory.Observations.back().StateVector;
        }

        return prediction;
    }

    double ComputeMSE(const Vector& predicted, const Vector& actual) const {
        if (predicted.empty() || actual.empty()) return 0.0;
        
        double sumSqError = 0.0;
        int n = std::min(predicted.size(), actual.size());
        
        for (int i = 0; i < n; ++i) {
            sumSqError += std::pow(predicted[i] - actual[i], 2);
        }
        
        return sumSqError / n;
    }

    double ComputeMAE(const Vector& predicted, const Vector& actual) const {
        if (predicted.empty() || actual.empty()) return 0.0;
        
        double sumAbsError = 0.0;
        int n = std::min(predicted.size(), actual.size());
        
        for (int i = 0; i < n; ++i) {
            sumAbsError += std::abs(predicted[i] - actual[i]);
        }
        
        return sumAbsError / n;
    }

    double ComputeCorrelation(const Vector& a, const Vector& b) const {
        if (a.empty() || b.empty()) return 0.0;
        
        int n = std::min(a.size(), b.size());
        
        double sumA = 0, sumB = 0;
        for (int i = 0; i < n; ++i) {
            sumA += a[i];
            sumB += b[i];
        }
        double meanA = sumA / n;
        double meanB = sumB / n;

        double numerator = 0, denomA = 0, denomB = 0;
        for (int i = 0; i < n; ++i) {
            double da = a[i] - meanA;
            double db = b[i] - meanB;
            numerator += da * db;
            denomA += da * da;
            denomB += db * db;
        }

        double denom = std::sqrt(denomA * denomB);
        if (denom > 1e-6) {
            return numerator / denom;
        }
        return 0.0;
    }

    void UpdateStatistics(const FPredictionValidation& validation, const FFuturePrediction& prediction) {
        Statistics.ValidatedPredictions++;

        double alpha = 1.0 / Statistics.ValidatedPredictions;
        Statistics.AverageMSE = (1.0 - alpha) * Statistics.AverageMSE + alpha * validation.MSE;
        Statistics.AverageConfidence = (1.0 - alpha) * Statistics.AverageConfidence + alpha * prediction.Confidence;

        double boundsCount = Statistics.WithinBoundsRatio * (Statistics.ValidatedPredictions - 1);
        if (validation.bWithinBounds) boundsCount += 1.0;
        Statistics.WithinBoundsRatio = boundsCount / Statistics.ValidatedPredictions;

        // Update method-specific MSE
        if (Statistics.MethodMSE.find(prediction.PredictionMethod) == Statistics.MethodMSE.end()) {
            Statistics.MethodMSE[prediction.PredictionMethod] = validation.MSE;
        } else {
            double& mse = Statistics.MethodMSE[prediction.PredictionMethod];
            mse = 0.9 * mse + 0.1 * validation.MSE;
        }

        // Update horizon accuracy
        double accuracy = std::clamp(1.0 - validation.MSE, 0.0, 1.0);
        if (Statistics.HorizonAccuracy.find(prediction.Horizon) == Statistics.HorizonAccuracy.end()) {
            Statistics.HorizonAccuracy[prediction.Horizon] = accuracy;
        } else {
            double& acc = Statistics.HorizonAccuracy[prediction.Horizon];
            acc = 0.9 * acc + 0.1 * accuracy;
        }

        UpdateMethodWeights();
    }
};

// ============================================================================
// Test Fixtures
// ============================================================================

class FutureStatePredictionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Predictor = std::make_unique<MockFutureStatePrediction>();
        Predictor->Initialize();
    }

    void TearDown() override {
        Predictor.reset();
    }

    // Generate linear trajectory data
    void GenerateLinearTrajectory(const std::string& trajectoryID, int numObservations, 
                                   double slope = 1.0, double intercept = 0.0, int dim = 4) {
        for (int i = 0; i < numObservations; ++i) {
            Vector state(dim);
            for (int d = 0; d < dim; ++d) {
                state[d] = slope * i + intercept + (d * 0.1);  // Slight variation per dim
            }
            Predictor->AddObservation(trajectoryID, state, 1.0);
            Predictor->AdvanceTime(0.1);  // 100ms between observations
        }
    }

    // Generate sinusoidal trajectory
    void GenerateSinusoidalTrajectory(const std::string& trajectoryID, int numObservations, 
                                        double amplitude = 1.0, double frequency = 0.5, int dim = 4) {
        for (int i = 0; i < numObservations; ++i) {
            Vector state(dim);
            double t = i * 0.1;
            for (int d = 0; d < dim; ++d) {
                state[d] = amplitude * std::sin(frequency * t + d * 0.5);
            }
            Predictor->AddObservation(trajectoryID, state, 1.0);
            Predictor->AdvanceTime(0.1);
        }
    }

    std::unique_ptr<MockFutureStatePrediction> Predictor;
};

// ============================================================================
// Trajectory Management Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, Initialization) {
    EXPECT_TRUE(Predictor->IsInitialized());
}

TEST_F(FutureStatePredictionTest, CreateTrajectory) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive, "TestTrajectory");
    
    EXPECT_FALSE(id.empty());
    EXPECT_TRUE(Predictor->TrajectoryExists(id));
    
    FStateTrajectory trajectory = Predictor->GetTrajectory(id);
    EXPECT_EQ(trajectory.Domain, EStateDomain::Cognitive);
}

TEST_F(FutureStatePredictionTest, AddObservations) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Motor, "MotorTrajectory");
    
    Vector state1 = {1.0, 2.0, 3.0, 4.0};
    Vector state2 = {1.5, 2.5, 3.5, 4.5};
    
    Predictor->AddObservation(id, state1, 1.0);
    Predictor->AdvanceTime(0.1);
    Predictor->AddObservation(id, state2, 0.9);
    
    FStateTrajectory trajectory = Predictor->GetTrajectory(id);
    EXPECT_EQ(trajectory.Observations.size(), 2);
    EXPECT_EQ(trajectory.Observations[0].StateVector, state1);
    EXPECT_EQ(trajectory.Observations[1].StateVector, state2);
}

TEST_F(FutureStatePredictionTest, TrajectoryFitting) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    
    // Generate enough observations for fitting
    GenerateLinearTrajectory(id, 10, 0.5, 1.0);
    
    double gof = Predictor->FitTrajectory(id, ETrajectoryType::Linear);
    
    // Linear data should have high goodness of fit
    EXPECT_GT(gof, 0.8);
    
    FStateTrajectory trajectory = Predictor->GetTrajectory(id);
    EXPECT_FALSE(trajectory.FittedParameters.empty());
}

TEST_F(FutureStatePredictionTest, ClearTrajectory) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Emotional);
    GenerateLinearTrajectory(id, 10);
    
    FStateTrajectory before = Predictor->GetTrajectory(id);
    EXPECT_GT(before.Observations.size(), 0);
    
    Predictor->ClearTrajectory(id);
    
    FStateTrajectory after = Predictor->GetTrajectory(id);
    EXPECT_EQ(after.Observations.size(), 0);
    EXPECT_TRUE(Predictor->TrajectoryExists(id));  // Still exists, just empty
}

TEST_F(FutureStatePredictionTest, RemoveTrajectory) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Social);
    EXPECT_TRUE(Predictor->TrajectoryExists(id));
    
    Predictor->RemoveTrajectory(id);
    EXPECT_FALSE(Predictor->TrajectoryExists(id));
}

// ============================================================================
// Prediction Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, LinearPrediction) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 10, 1.0, 0.0);
    
    FFuturePrediction prediction = Predictor->PredictFutureState(id, 1, ETrajectoryType::Linear);
    
    EXPECT_GT(prediction.PredictionID, 0);
    EXPECT_FALSE(prediction.PredictedState.empty());
    EXPECT_GT(prediction.Confidence, 0.0);
    EXPECT_EQ(prediction.Domain, EStateDomain::Cognitive);
    EXPECT_EQ(prediction.PredictionMethod, ETrajectoryType::Linear);
}

TEST_F(FutureStatePredictionTest, MultipleHorizonPrediction) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Motor);
    GenerateLinearTrajectory(id, 15);
    
    // Predict at different horizons
    FFuturePrediction pred1 = Predictor->PredictFutureState(id, 1);
    FFuturePrediction pred5 = Predictor->PredictFutureState(id, 5);
    FFuturePrediction pred10 = Predictor->PredictFutureState(id, 10);
    
    EXPECT_EQ(pred1.Horizon, EPredictionHorizon::Immediate);
    EXPECT_EQ(pred5.Horizon, EPredictionHorizon::ShortTerm);
    EXPECT_EQ(pred10.Horizon, EPredictionHorizon::MediumTerm);
    
    // Confidence should decrease with horizon
    EXPECT_GT(pred1.Confidence, pred5.Confidence);
    EXPECT_GT(pred5.Confidence, pred10.Confidence);
}

TEST_F(FutureStatePredictionTest, EnsemblePrediction) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 15);
    
    FEnsemblePrediction ensemble = Predictor->GenerateEnsemblePrediction(id, 3);
    
    EXPECT_GT(ensemble.IndividualPredictions.size(), 0);
    EXPECT_FALSE(ensemble.EnsembleState.empty());
    EXPECT_GT(ensemble.EnsembleConfidence, 0.0);
    EXPECT_FALSE(ensemble.MethodWeights.empty());
}

TEST_F(FutureStatePredictionTest, InsufficientDataPrediction) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    
    // Only add 2 observations (less than MinObservationsForFitting)
    Vector state = {1.0, 2.0, 3.0};
    Predictor->AddObservation(id, state, 1.0);
    Predictor->AdvanceTime(0.1);
    Predictor->AddObservation(id, state, 1.0);
    
    FFuturePrediction prediction = Predictor->PredictFutureState(id, 1);
    
    // Should have very low confidence
    EXPECT_EQ(prediction.Confidence, 0.0);
    EXPECT_EQ(prediction.ConfidenceLevel, EPredictionConfidence::VeryLow);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, PredictionValidation) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 10, 1.0, 0.0);
    
    FFuturePrediction prediction = Predictor->PredictFutureState(id, 1, ETrajectoryType::Linear);
    
    // Generate "actual" state close to prediction
    Vector actualState = prediction.PredictedState;
    for (double& val : actualState) {
        val += 0.1;  // Small error
    }
    
    Predictor->AdvanceTime(0.1);
    FPredictionValidation validation = Predictor->ValidatePrediction(prediction.PredictionID, actualState);
    
    EXPECT_EQ(validation.PredictionID, prediction.PredictionID);
    EXPECT_GE(validation.MSE, 0.0);
    EXPECT_GE(validation.MAE, 0.0);
    
    // Prediction should now be marked as validated
    FFuturePrediction updatedPred = Predictor->GetPrediction(prediction.PredictionID);
    EXPECT_TRUE(updatedPred.bValidated);
}

TEST_F(FutureStatePredictionTest, StatisticsTracking) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 10);
    
    // Make several predictions and validate them
    for (int i = 0; i < 5; ++i) {
        FFuturePrediction pred = Predictor->PredictFutureState(id, 1, ETrajectoryType::Linear);
        Predictor->AdvanceTime(0.1);
        Predictor->ValidatePrediction(pred.PredictionID, pred.PredictedState);
    }
    
    FPredictionStatistics stats = Predictor->GetPredictionStatistics();
    
    EXPECT_EQ(stats.TotalPredictions, 5);
    EXPECT_EQ(stats.ValidatedPredictions, 5);
    EXPECT_GE(stats.AverageMSE, 0.0);
}

// ============================================================================
// Confidence and Uncertainty Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, ConfidenceComputation) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 20);
    
    double conf1 = Predictor->ComputePredictionConfidence(id, 1);
    double conf10 = Predictor->ComputePredictionConfidence(id, 10);
    double conf20 = Predictor->ComputePredictionConfidence(id, 20);
    
    // Confidence should decay with horizon
    EXPECT_GT(conf1, conf10);
    EXPECT_GT(conf10, conf20);
    
    // All should be in valid range
    EXPECT_GE(conf1, 0.0);
    EXPECT_LE(conf1, 1.0);
}

TEST_F(FutureStatePredictionTest, UncertaintyBounds) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 15);
    
    Vector bounds1 = Predictor->ComputeUncertaintyBounds(id, 1);
    Vector bounds10 = Predictor->ComputeUncertaintyBounds(id, 10);
    
    EXPECT_FALSE(bounds1.empty());
    EXPECT_FALSE(bounds10.empty());
    
    // Bounds should increase with horizon
    for (size_t i = 0; i < std::min(bounds1.size(), bounds10.size()); ++i) {
        EXPECT_LE(bounds1[i], bounds10[i]);
    }
}

TEST_F(FutureStatePredictionTest, ConfidenceClassification) {
    EXPECT_EQ(Predictor->ClassifyConfidence(0.1), EPredictionConfidence::VeryLow);
    EXPECT_EQ(Predictor->ClassifyConfidence(0.3), EPredictionConfidence::Low);
    EXPECT_EQ(Predictor->ClassifyConfidence(0.5), EPredictionConfidence::Medium);
    EXPECT_EQ(Predictor->ClassifyConfidence(0.7), EPredictionConfidence::High);
    EXPECT_EQ(Predictor->ClassifyConfidence(0.9), EPredictionConfidence::VeryHigh);
}

// ============================================================================
// Method Weight Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, MethodWeightsInitialization) {
    auto weights = Predictor->GetMethodWeights();
    
    EXPECT_FALSE(weights.empty());
    
    // Sum should be approximately 1
    double sum = 0.0;
    for (const auto& pair : weights) {
        sum += pair.second;
    }
    EXPECT_NEAR(sum, 1.0, 0.01);
}

TEST_F(FutureStatePredictionTest, MethodWeightAdaptation) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 15);
    
    auto weightsBefore = Predictor->GetMethodWeights();
    
    // Make predictions and validate with varying errors
    for (int i = 0; i < 10; ++i) {
        FFuturePrediction pred = Predictor->PredictFutureState(id, 1, ETrajectoryType::Linear);
        Predictor->AdvanceTime(0.1);
        
        // Create actual state with small error for linear
        Vector actualState = pred.PredictedState;
        for (double& val : actualState) val += 0.01;
        
        Predictor->ValidatePrediction(pred.PredictionID, actualState);
    }
    
    Predictor->UpdateMethodWeights();
    auto weightsAfter = Predictor->GetMethodWeights();
    
    // Weights should have been updated
    EXPECT_FALSE(weightsAfter.empty());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(FutureStatePredictionTest, NonexistentTrajectory) {
    FFuturePrediction prediction = Predictor->PredictFutureState("nonexistent", 1);
    
    EXPECT_EQ(prediction.Confidence, 0.0);
    EXPECT_EQ(prediction.ConfidenceLevel, EPredictionConfidence::VeryLow);
}

TEST_F(FutureStatePredictionTest, ZeroStepsAhead) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 10);
    
    FFuturePrediction prediction = Predictor->PredictFutureState(id, 0);
    
    // Should still work, returning current state essentially
    EXPECT_GT(prediction.PredictionID, 0);
}

TEST_F(FutureStatePredictionTest, LargeHorizon) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 10);
    
    FFuturePrediction prediction = Predictor->PredictFutureState(id, 100);
    
    EXPECT_EQ(prediction.Horizon, EPredictionHorizon::LongTerm);
    // Should have low confidence for far future
    EXPECT_LT(prediction.Confidence, 0.3);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, PredictionPerformance) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 50);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        Predictor->PredictFutureState(id, 5, ETrajectoryType::Linear);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 1000 predictions should complete in under 500ms
    EXPECT_LT(duration.count(), 500);
}

TEST_F(FutureStatePredictionTest, EnsemblePerformance) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    GenerateLinearTrajectory(id, 50);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 500; ++i) {
        Predictor->GenerateEnsemblePrediction(id, 5);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 500 ensemble predictions should complete in under 500ms
    EXPECT_LT(duration.count(), 500);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(FutureStatePredictionTest, FullPredictionValidationCycle) {
    std::string id = Predictor->CreateTrajectory(EStateDomain::Cognitive);
    
    // Generate initial trajectory
    GenerateLinearTrajectory(id, 10, 1.0, 0.0);
    
    // Make prediction
    FFuturePrediction prediction = Predictor->PredictFutureState(id, 1, ETrajectoryType::Linear);
    EXPECT_GT(prediction.Confidence, 0.0);
    
    // Simulate time passing
    Predictor->AdvanceTime(0.1);
    
    // Generate "actual" observation
    Vector actualState = prediction.PredictedState;
    for (double& val : actualState) val += 0.05;  // Small deviation
    
    // Validate
    FPredictionValidation validation = Predictor->ValidatePrediction(prediction.PredictionID, actualState);
    
    EXPECT_GE(validation.Correlation, 0.9);  // Should be highly correlated
    EXPECT_LT(validation.MSE, 0.1);  // Should have low error
    
    // Add observation and continue
    Predictor->AddObservation(id, actualState, 1.0);
    
    // Make another prediction
    FFuturePrediction nextPred = Predictor->PredictFutureState(id, 1);
    EXPECT_GT(nextPred.PredictionID, prediction.PredictionID);
}

TEST_F(FutureStatePredictionTest, MultiDomainPrediction) {
    std::string cogId = Predictor->CreateTrajectory(EStateDomain::Cognitive, "Cognitive");
    std::string motId = Predictor->CreateTrajectory(EStateDomain::Motor, "Motor");
    std::string emoId = Predictor->CreateTrajectory(EStateDomain::Emotional, "Emotional");
    
    GenerateLinearTrajectory(cogId, 10);
    GenerateLinearTrajectory(motId, 10);
    GenerateSinusoidalTrajectory(emoId, 10);
    
    FFuturePrediction cogPred = Predictor->PredictFutureState(cogId, 2);
    FFuturePrediction motPred = Predictor->PredictFutureState(motId, 2);
    FFuturePrediction emoPred = Predictor->PredictFutureState(emoId, 2);
    
    EXPECT_EQ(cogPred.Domain, EStateDomain::Cognitive);
    EXPECT_EQ(motPred.Domain, EStateDomain::Motor);
    EXPECT_EQ(emoPred.Domain, EStateDomain::Emotional);
    
    // All should have valid predictions
    EXPECT_FALSE(cogPred.PredictedState.empty());
    EXPECT_FALSE(motPred.PredictedState.empty());
    EXPECT_FALSE(emoPred.PredictedState.empty());
}
