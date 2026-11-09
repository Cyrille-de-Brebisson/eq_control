#include <cmath>
#include <iomanip>
#include <algorithm>
#include "asteroids.h"

constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double OBLIQUITY_J2000 = 23.439291 * DEG2RAD; // Obliquity of the ecliptic


// Truncated VSOP87 Earth position generator
// Accuracy: ~1-2 arcseconds relative to JPL DE440 for 1900-2100.
// Memory footprint: ~1 KB code/flash size. Zero external dependencies.
void get_earth_position_vsop87_truncated(double jd, double &xe_ecl, double &ye_ecl, double &ze_ecl) 
{
    // T = Julian Centuries from J2000.0 (TDB/TT)
    const double T = (jd - 2451545.0) / 36525.0;

    // --- Mean Anomalies and Longitudes of Major Planets (Radians) ---
    const double L_Mercury = std::fmod(252.2509 + 149472.6741 * T, 360.0) * DEG2RAD;
    const double L_Venus = std::fmod(181.9798 + 58517.8156 * T, 360.0) * DEG2RAD;
    const double L_Earth = std::fmod(100.4664 + 35999.3729 * T, 360.0) * DEG2RAD;
    const double L_Mars = std::fmod(355.4330 + 19140.2993 * T, 360.0) * DEG2RAD;
    const double L_Jupiter = std::fmod(34.4044 + 3034.9057 * T, 360.0) * DEG2RAD;
    const double L_Saturn = std::fmod(50.0774 + 1222.1138 * T, 360.0) * DEG2RAD;

    // Moon mean elongation (D) and mean anomaly (M_prime)
    const double D = std::fmod(297.8502 + 445267.1115 * T, 360.0) * DEG2RAD;
    const double M_prime = std::fmod(134.9634 + 477198.8676 * T, 360.0) * DEG2RAD;

    // --- Earth Mean Longitude L0, Eccentricity e, and Perihelion pi ---
    const double L0 = (100.466457 + 36000.769828 * T + 0.0003032 * T * T) * DEG2RAD;
    const double M = (357.529109 + 35999.050291 * T - 0.0001537 * T * T) * DEG2RAD;

    // --- 1. Heliocentric Longitude L (Radians) ---
    // Unperturbed Keplerian Equation of Center
    double L = L0 + (1.914600 - 0.004817 * T) * DEG2RAD * std::sin(M) + (0.019993 - 0.000101 * T) * DEG2RAD * std::sin(2.0 * M) + 0.000289 * DEG2RAD * std::sin(3.0 * M);

    // Planetary Perturbations on Earth's Longitude (Venus, Jupiter, Moon, Saturn)
    L += 0.001340 * DEG2RAD * std::cos(L_Jupiter - L_Earth);
    L += 0.001540 * DEG2RAD * std::sin(L_Jupiter - L_Earth);
    L += 0.002000 * DEG2RAD * std::cos(2.0 * (L_Venus - L_Earth));
    L += 0.001790 * DEG2RAD * std::sin(2.0 * (L_Venus - L_Earth));
    L += 0.001780 * DEG2RAD * std::sin(L_Venus - L_Earth);
    L += 0.000540 * DEG2RAD * std::sin(L_Mars - L_Earth);
    L += 0.000430 * DEG2RAD * std::sin(L_Saturn - L_Earth);
    L += 0.000310 * DEG2RAD * std::sin(2.0 * (L_Jupiter - L_Earth));

    // Earth-Moon Barycenter Offset (Lunar Perturbation on Earth Position)
    L += 0.000030 * DEG2RAD * std::sin(M_prime);
    L += 0.000018 * DEG2RAD * std::sin(2.0 * D - M_prime);

    // --- 2. Heliocentric Radius Vector R (AU) ---
    // Unperturbed Keplerian Distance
    double R = 1.0001406 - (0.0167086 - 0.0000420 * T) * std::cos(M) - 0.0001395 * std::cos(2.0 * M);

    // Planetary & Lunar Perturbations on Radius
    R += 0.0000308 * std::cos(L_Venus - L_Earth);
    R += 0.0000307 * std::cos(2.0 * (L_Venus - L_Earth));
    R += 0.0000224 * std::cos(L_Jupiter - L_Earth);
    R += 0.0000072 * std::cos(2.0 * (L_Jupiter - L_Earth));
    R += 0.0000013 * std::cos(M_prime); // Lunar effect

    // --- 3. Heliocentric Latitude B (Radians) ---
    // Earth's orbit is near the ecliptic J2000 plane, but perturbations tilt it slightly
    double B = 0.0;
    B += 0.000011 * DEG2RAD * std::sin(L_Venus - L_Earth);
    B += 0.000006 * DEG2RAD * std::sin(L_Jupiter - L_Earth);

    // --- 4. Convert Spherical (L, B, R) to Cartesian (x, y, z) in AU ---
    xe_ecl = R * std::cos(B) * std::cos(L);
    ye_ecl = R * std::cos(B) * std::sin(L);
    ze_ecl = R * std::sin(B);
}

// int main() {
//     // Example: October 9, 2026 12:00 UTC (Julian Date: 2461323.0)
//     double target_jd = 2461323.0;
// 
//     Vector3D earth_pos = get_earth_position_vsop87_truncated(target_jd);
// 
//     std::cout << std::fixed << std::setprecision(8);
//     std::cout << "Earth Heliocentric Ecliptic Position (AU):\n";
//     std::cout << "X: " << earth_pos.x << " AU\n";
//     std::cout << "Y: " << earth_pos.y << " AU\n";
//     std::cout << "Z: " << earth_pos.z << " AU\n";
// 
//     double distance = std::sqrt(earth_pos.x * earth_pos.x +
//         earth_pos.y * earth_pos.y +
//         earth_pos.z * earth_pos.z);
//     std::cout << "Distance from Sun: " << distance << " AU\n";
// 
//     return 0;
// }

// How to Drop This into the MPC Ephemeris CodeTo integrate this directly into the earlier asteroid ephemeris code : Replace the old get_earth_position_ecliptic() function with get_earth_position_vsop87_truncated().Notice that the output is in Heliocentric Ecliptic Cartesian Coordinates $(X, Y, Z)$ in AU.Apply the obliquity of the ecliptic($\epsilon$) to convert Earth's coordinate vector to Equatorial before subtracting it from the asteroid's heliocentric position : $$\begin{ aligned } X_{ eq } &= X_{ ecl } \\ Y_{ eq } &= Y_{ ecl } \cos \epsilon - Z_{ ecl } \sin \epsilon \\ Z_{ eq } &= Y_{ ecl } \sin \epsilon + Z_{ ecl } \cos \epsilon \end{ aligned }$$


// Converts MPC packed 5-character epoch (e.g., "K24A1") to Julian Date
// MPC packed format: Byte 1 = Century (I=1800, J=1900, K=2000), Bytes 2-3 = Year, Byte 4 = Month, Byte 5 = Day
double parse_mpc_packed_epoch(const std::string& packed) 
{
    if (packed.length() < 5) return 2451545.0; // Default J2000

    int century = 2000;
    if (packed[0] == 'I') century = 1800;
    else if (packed[0] == 'J') century = 1900;
    else if (packed[0] == 'K') century = 2000;

    int year = century + std::stoi(packed.substr(1, 2));

    auto parse_base62 = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
        if (c >= 'a' && c <= 'z') return c - 'a' + 36;
        return 0;
        };

    int month = parse_base62(packed[3]);
    int day = parse_base62(packed[4]);

    // Fliegel-Van Flandern algorithm for Julian Date
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045 - 0.5;
}

// Converts Calendar Date (UTC) to Julian Date
double calendar_to_jd(int year, int month, double day) 
{
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045 - 0.5;
}

// Solves Kepler's Equation M = E - e * sin(E) using Newton-Raphson iteration
double solve_kepler(double M_rad, double e) 
{
    double E = M_rad; // Initial guess
    for (int i = 0; i < 100; ++i) 
    {
        double f = E - e * std::sin(E) - M_rad;
        double f_prime = 1.0 - e * std::cos(E);
        double delta = f / f_prime;
        E -= delta;
        if (std::abs(delta) < 1e-12) break;
    }
    return E;
}

// High-precision WGS84 Topocentric Observer Offset Vector (AU)
void get_topocentric_offset_wgs84(double jd, double lat_deg, double lon_deg, double elevation_m, double& dx, double& dy, double& dz) 
{
    constexpr double a_m = 6378137.0;             // WGS84 Semi-major axis in meters
    constexpr double e2 = 0.00669437999014;       // WGS84 First eccentricity squared
    constexpr double meters_to_au = 1.0 / 149597870700.0;

    double phi = lat_deg * DEG2RAD;
    double sin_phi = std::sin(phi);
    double cos_phi = std::cos(phi);

    // Radius of curvature in the prime vertical
    double N = a_m / std::sqrt(1.0 - e2 * sin_phi * sin_phi);

    // Compute geocentric distance components (XY plane and Z axis)
    double r_xy_m = (N + elevation_m) * cos_phi;
    double r_z_m = (N * (1.0 - e2) + elevation_m) * sin_phi;

    // IAU High-Precision Greenwich Mean Sidereal Time (GMST)
    double T = (jd - 2451545.0) / 36525.0;
    double gmst_sec = 24110.54841 + 8640184.812866 * T + 0.093104 * T * T - 6.2e-6 * T * T * T;

    // Add time of day fraction
    double day_fraction = std::fmod(jd + 0.5, 1.0);
    gmst_sec += day_fraction * 86400.0 * 1.002737909350795;

    double gmst_deg = std::fmod(gmst_sec * (360.0 / 86400.0), 360.0);
    if (gmst_deg < 0) gmst_deg += 360.0;

    // Local Mean Sidereal Time (LMST) in radians
    double lmst_rad = (gmst_deg + lon_deg) * DEG2RAD;

    // Topocentric offset vector in Equatorial AU
    dx = (r_xy_m * std::cos(lmst_rad)) * meters_to_au;
    dy = (r_xy_m * std::sin(lmst_rad)) * meters_to_au;
    dz = (r_z_m)*meters_to_au;
}

//// Topocentric Observer Position Vector relative to Earth's Center (Equatorial Frame in AU)
//void get_topocentric_offset(double jd, double lat_deg, double lon_deg, double elevation_m, double& dx, double& dy, double& dz) 
//{
//    constexpr double a_earth_au = 6378137.0 / 149597870700.0; // Earth radius in AU
//    double lat_rad = lat_deg * DEG2RAD;
//
//    // Calculate Greenwich Mean Sidereal Time (GMST)
//    double D = jd - 2451545.0;
//    double gmst_deg = std::fmod(280.46061837 + 360.98564736629 * D, 360.0);
//    if (gmst_deg < 0) gmst_deg += 360.0;
//
//    double lmst_rad = (gmst_deg + lon_deg) * DEG2RAD; // Local Mean Sidereal Time
//
//    double r_obs = a_earth_au + (elevation_m / 149597870700.0);
//    dx = r_obs * std::cos(lat_rad) * std::cos(lmst_rad);
//    dy = r_obs * std::cos(lat_rad) * std::sin(lmst_rad);
//    dz = r_obs * std::sin(lat_rad);
//}

// Parses a single line from MPCORB.DAT
bool parse_mpcorb_line(const std::string& line, CMPCOrbit& orb) 
{
    if (line.length() < 103) return false;

    orb.designation = line.substr(166, 28);
    // Trim spaces
    orb.designation.erase(orb.designation.find_last_not_of(" \n\r\t") + 1);
    int TODO; // error management
    orb.H = std::stod(line.substr(25, 5));
    orb.G = std::stod(line.substr(31, 5));
    orb.epoch_jd = parse_mpc_packed_epoch(line.substr(20, 5));
    orb.M = std::stod(line.substr(26, 9));
    orb.arg_peri = std::stod(line.substr(37, 9));
    orb.node = std::stod(line.substr(48, 9));
    orb.inc = std::stod(line.substr(59, 9));
    orb.e = std::stod(line.substr(70, 9));
    orb.n = std::stod(line.substr(80, 11));
    orb.a = std::stod(line.substr(92, 11));
    return true;
}
bool CMPCOrbit::fromMpcorb(const std::string& line)
{
    return parse_mpcorb_line(line, *this);
}

// Calculates Topocentric Right Ascension (RA) and Declination (Dec)
void calculate_ephemeris(const CMPCOrbit& orb, double target_jd, double lat_deg, double lon_deg, double elev_m, double& ra_deg, double& dec_deg, double& dist_au) 
{
    // 1. Advance Mean Anomaly to Target JD
    double dt = target_jd - orb.epoch_jd;
    double M_current = std::fmod(orb.M + orb.n * dt, 360.0);
    if (M_current < 0) M_current += 360.0;
    double M_rad = M_current * DEG2RAD;

    // 2. Solve Kepler's Equation for Eccentric Anomaly (E)
    double E_rad = solve_kepler(M_rad, orb.e);

    // 3. Compute Position in Orbital Plane
    double x_orb = orb.a * (std::cos(E_rad) - orb.e);
    double y_orb = orb.a * std::sqrt(1.0 - orb.e * orb.e) * std::sin(E_rad);

    // 4. Transform to Heliocentric Ecliptic Coordinates
    double w = orb.arg_peri * DEG2RAD;
    double O = orb.node * DEG2RAD;
    double i = orb.inc * DEG2RAD;

    double x_ecl = (std::cos(w) * std::cos(O) - std::sin(w) * std::sin(O) * std::cos(i)) * x_orb + (-std::sin(w) * std::cos(O) - std::cos(w) * std::sin(O) * std::cos(i)) * y_orb;
    double y_ecl = (std::cos(w) * std::sin(O) + std::sin(w) * std::cos(O) * std::cos(i)) * x_orb + (-std::sin(w) * std::sin(O) + std::cos(w) * std::cos(O) * std::cos(i)) * y_orb;
    double z_ecl = (std::sin(w) * std::sin(i)) * x_orb + (std::cos(w) * std::sin(i)) * y_orb;

    // 5. Transform Heliocentric Ecliptic to Heliocentric Equatorial Coordinates
    double x_helio_eq = x_ecl;
    double y_helio_eq = y_ecl * std::cos(OBLIQUITY_J2000) - z_ecl * std::sin(OBLIQUITY_J2000);
    double z_helio_eq = y_ecl * std::sin(OBLIQUITY_J2000) + z_ecl * std::cos(OBLIQUITY_J2000);

    // 6. Get Earth Heliocentric Ecliptic Position and convert to Equatorial
    double xe_ecl, ye_ecl, ze_ecl;
    get_earth_position_vsop87_truncated(target_jd, xe_ecl, ye_ecl, ze_ecl);

    double xe_eq = xe_ecl;
    double ye_eq = ye_ecl * std::cos(OBLIQUITY_J2000) - ze_ecl * std::sin(OBLIQUITY_J2000);
    double ze_eq = ye_ecl * std::sin(OBLIQUITY_J2000) + ze_ecl * std::cos(OBLIQUITY_J2000);

    // 7. Get Topocentric Observer Offset
    double dx_top, dy_top, dz_top;
    get_topocentric_offset_wgs84(target_jd, lat_deg, lon_deg, elev_m, dx_top, dy_top, dz_top);

    // 8. Calculate Geocentric -> Topocentric Vector
    double rx = x_helio_eq - (xe_eq + dx_top);
    double ry = y_helio_eq - (ye_eq + dy_top);
    double rz = z_helio_eq - (ze_eq + dz_top);

    dist_au = std::sqrt(rx * rx + ry * ry + rz * rz);

    // 9. Compute Right Ascension and Declination
    ra_deg = std::atan2(ry, rx) * RAD2DEG/15.0f;
    if (ra_deg < 0) ra_deg += 24.0;

    dec_deg = std::asin(rz / dist_au) * RAD2DEG;
}

void CMPCOrbit::ephemeris(double target_jd, double lat_deg, double lon_deg, double elev_m, double& ra_deg, double& dec_deg)
{
    double t;
    calculate_ephemeris(*this, target_jd, lat_deg, lon_deg, elev_m, ra_deg, dec_deg, t);
}

/*
int main() {
    // Example: Ceres (MPC Record Line Format)
    std::string sample_mpc_line =
        "00001    3.34  0.15 K2431 233.15195  151.44286   80.25892   10.58737  0.0789121  0.21404172   2.7664923  0 M 703B1   4819 107  181 days 0.18 M-v 38h MPCLINUX   0000      (1) Ceres              20240315";

    MPCOrbit ceres;
    if (!parse_mpcorb_line(sample_mpc_line, ceres)) {
        std::cerr << "Failed to parse MPC line.\n";
        return 1;
    }

    // Observer Location (e.g., Lyon, France: 45.76° N, 4.83° E, 170m elev)
    double obs_lat = 45.76;
    double obs_lon = 4.83;
    double obs_elev = 170.0;

    // Target Time (e.g., UTC Oct 9, 2026, 12:00 UTC)
    double target_jd = calendar_to_jd(2026, 10, 9.5);

    double ra, dec, dist;
    calculate_ephemeris(ceres, target_jd, obs_lat, obs_lon, obs_elev, ra, dec, dist);

    // Convert RA to Hours, Minutes, Seconds
    int ra_hours = static_cast<int>(ra);
    int ra_min = static_cast<int>((ra - ra_hours) * 60.0);
    double ra_sec = ((ra - ra_hours) * 60.0 - ra_min) * 60.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Target: " << ceres.designation << "\n";
    std::cout << "Julian Date: " << target_jd << "\n";
    std::cout << "Distance: " << dist << " AU\n";
    std::cout << "RA: " << ra_hours << "h " << ra_min << "m " << ra_sec << "s (" << ra << " deg)\n";
    std::cout << "Dec: " << dec << " deg\n";

    return 0;
}*/