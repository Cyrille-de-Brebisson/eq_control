#include <string>
struct CMPCOrbit {
    bool fromMpcorb(const std::string& line);
    std::string designation;
    double epoch_jd;    // Epoch in Julian Date
    double M;           // Mean anomaly at epoch (degrees)
    double arg_peri;    // Argument of perihelion (degrees)
    double node;        // Longitude of ascending node (degrees)
    double inc;         // Inclination (degrees)
    double e;           // Eccentricity
    double n;           // Mean daily motion (degrees/day)
    double a;           // Semi-major axis (AU)
    double H;           // Absolute magnitude
    double G;           // Slope parameter
    void ephemeris(double target_jd, double lat_deg, double lon_deg, double elev_m, double& ra_deg, double& dec_deg);
};

