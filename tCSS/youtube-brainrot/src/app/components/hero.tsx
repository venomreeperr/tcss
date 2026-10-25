import React from "react";

export const Hero = () => {
    return (
        <div className="hero-root">
            <div className="badge">
                <span>What are form 1099 filing requirements?</span>
                <svg
                    viewBox="0 0 16 16"
                    width="14"
                    height="14"
                    fill="none"
                    className="badge-icon"
                >
                    <path
                        stroke="#1E1F25"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                        strokeWidth="1.25"
                        d="M8 4.75 11.25 8m0 0L8 11.25M11.25 8h-6.5"
                    />
                </svg>
            </div>
            <h1 className="hero-title">Magically simplify accounting and taxes</h1>
            <p className="hero-subtitle">
                Automated bookkeeping. Effortless tax filing. Financial clarity. Set up in 10 mins. Back to building by 12:04am
            </p>

            <div className="hero-cta">
                <button className="btn btn-primary">Start free trial</button>
                <button className="btn btn-secondary">
                    <span>Pricing</span>
                    <svg
                        viewBox="0 0 16 16"
                        width="14"
                        height="14"
                        fill="none"
                        className="badge-icon"
                    >
                        <path
                            stroke="#1E1F25"
                            strokeLinecap="round"
                            strokeLinejoin="round"
                            strokeOpacity="0.7"
                            strokeWidth="1.25"
                            d="M8 4.75 11.25 8m0 0L8 11.25M11.25 8h-6.5"
                        />
                    </svg>
                </button>
            </div>
        </div>
    );
};
