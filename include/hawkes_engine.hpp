#pragma once

#include "spin_loop_engine.hpp"

namespace hft {

class HawkesIntensityEngine {
public:
    HawkesIntensityEngine(double mu_buy = 10.0, double mu_sell = 10.0,
                         double alpha_self = 0.5, double alpha_cross = 0.2,
                         double beta = 1e-3, double gamma = 1.5, size_t max_history = 1000)
        : mu_buy_(mu_buy), mu_sell_(mu_sell), alpha_self_(alpha_self), alpha_cross_(alpha_cross),
          beta_(beta), intensity_buy_(mu_buy), intensity_sell_(mu_sell),
          state_buy_(0.0), state_sell_(0.0),
          current_time_(now()) {
        
        if (beta_ <= 0) beta_ = 1e-3;
    }

    void update(const TradingEvent& event) {
        const double dt = (to_nanos(event.arrival_time) - to_nanos(current_time_)) * 1e-9;
        
        // Decay existing state
        const double decay = hft::spin_loop::fast_exp(-beta_ * dt);
        state_buy_ *= decay;
        state_sell_ *= decay;
        
        // Add new event
        if (event.event_type == Side::BUY) {
            state_buy_ += 1.0;
        } else {
            state_sell_ += 1.0;
        }

        current_time_ = event.arrival_time;
        
        // Calculate intensities: lambda = mu + alpha_self * state + alpha_cross * state_other
        // Note: alpha here should be pre-multiplied by beta for standard Hawkes intensity
        intensity_buy_ = mu_buy_ + (alpha_self_ * beta_ * state_buy_) + (alpha_cross_ * beta_ * state_sell_);
        intensity_sell_ = mu_sell_ + (alpha_self_ * beta_ * state_sell_) + (alpha_cross_ * beta_ * state_buy_);
    }

    double get_buy_intensity() const { return intensity_buy_; }
    double get_sell_intensity() const { return intensity_sell_; }

    double get_intensity_imbalance() const {
        const double total = intensity_buy_ + intensity_sell_;
        if (total < 1e-10) return 0.0;
        return (intensity_buy_ - intensity_sell_) / total;
    }

    double predict_buy_intensity(Duration forecast_horizon) const {
        const double dt = forecast_horizon.count() * 1e-9;
        const double decay = hft::spin_loop::fast_exp(-beta_ * dt);
        const double future_state_buy = state_buy_ * decay;
        const double future_state_sell = state_sell_ * decay;
        
        return mu_buy_ + (alpha_self_ * beta_ * future_state_buy) + (alpha_cross_ * beta_ * future_state_sell);
    }

    double predict_sell_intensity(Duration forecast_horizon) const {
        const double dt = forecast_horizon.count() * 1e-9;
        const double decay = hft::spin_loop::fast_exp(-beta_ * dt);
        const double future_state_buy = state_buy_ * decay;
        const double future_state_sell = state_sell_ * decay;
        
        return mu_sell_ + (alpha_self_ * beta_ * future_state_sell) + (alpha_cross_ * beta_ * future_state_buy);
    }

    void reset() {
        state_buy_ = 0.0;
        state_sell_ = 0.0;
        intensity_buy_ = mu_buy_;
        intensity_sell_ = mu_sell_;
        current_time_ = now();
    }

private:
    double mu_buy_;
    double mu_sell_;
    double alpha_self_;
    double alpha_cross_;
    double beta_;

    double intensity_buy_;
    double intensity_sell_;
    double state_buy_;   // sum of e^-beta(t-ti)
    double state_sell_;  
    Timestamp current_time_;
};

// Aliases for compatibility
using HawkesEngine = HawkesIntensityEngine;

}