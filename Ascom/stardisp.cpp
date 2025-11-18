
//////////////////////////////////////////////
// Planetary stuff
float timeFromDate(int y, int m, int d, int h, int M, int s)
{
    return float(367*y - 7 * ( y + (m+9)/12 ) / 4 + 275*m/9 + d - 730530) + (h+M/60.0f+s/3600.0f)/24.0f;
}

//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
struct TOrbitalElements { float N1, N2, i1, i2, w1, w2, a1, a2, e1, e2, M1, M2; };
class TOrbitalElementsAt { public:
    float N, i, w, a, e, M; 
    TOrbitalElementsAt(TOrbitalElements const &o, float d)
    { 
      N= o.N1+o.N2*d; i=o.i1+o.i2*d; w= o.w1+o.w2*d;
      a= o.a1+o.a2*d; e= o.e1+o.e2*d; M= o.M1+o.M1*d;
    }
};
static TOrbitalElements const oe[9]={
    // N                    i                  w                     a                   e                    M
    {0, 0,                  0, 0,              282.9404, 4.70935E-5, 1, 0,               0.016709, -1.151E-9, 356.0470, 0.9856002585}, // sun
    {48.3313, 3.24587E-5,   7.0047, 5.00E-8,   29.1241, 1.01444E-5,  0.387098,0,         0.205635, 5.59E-10,  168.6562, 4.0923344368}, // mercury
    {76.6799, 2.46590E-5,   3.3946, 2.75E-8,   54.8910, 1.38374E-5,  0.723330,0,         0.006773, 1.302E-9,  48.0052, 1.6021302244}, // Venus
    { 49.5574, 2.11081E-5,  1.8497, -1.78E-8,  286.5016, 2.92961E-5, 1.523688,0,         0.093405, 2.516E-9,  18.6021, 0.5240207766}, // Mars
    { 100.4542, 2.76854E-5, 1.3030, -1.557E-7, 273.8777, 1.64505E-5, 5.20256,0,          0.048498, 4.469E-9,  19.8950, 0.0830853001}, // Jupiter
    { 113.6634, 2.38980E-5, 2.4886, -1.081E-7, 339.3939, 2.97661E-5, 9.55475,0,          0.055546, -9.499E-9, 316.9670, 0.0334442282}, // Saturne
    { 74.0005, 1.3978E-5,   0.7733, 1.9E-8,    96.6612, 3.0565E-5,   19.18171, -1.55E-8, 0.047318, 7.45E-9,   142.5905, 0.011725806}, // Uranus
    { 131.7806, 3.0173E-5,  1.7700, -2.55E-7,  272.8461, -6.027E-6,  30.05826, 3.313E-8, 0.008606, 2.15E-9,   260.2471, 0.005995147} // Nepture
};

void sunPos(float d, float &RA, float &Dec)
{
    float ecl= 23.4393 - 3.563E-7 * d; // ecl angle
    TOrbitalElementsAt sun(oe[0], d);
    float E= sun.M+sun.e*(180.0f/MPI)*sinf(sun.M*degToRad)*(1.0+sun.e*cosf(sun.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    // compute the Sun's distance r and its true anomaly v from:
    float xv = cosf(E)-sun.e;
    float yv = sqrt(1.0f-sun.e*sun.e) * sinf(E);
    float v = atan2f( yv, xv );
    float rs = sqrt( xv*xv + yv*yv );
    //compute the Sun's true longitude
    float lonsun = v + sun.w*degToRad;
    // Convert lonsun,r to ecliptic rectangular geocentric coordinates xs,ys:
    float xs = rs * cosf(lonsun);
    float ys = rs * sinf(lonsun);
    // (since the Sun always is in the ecliptic plane, zs is of course zero). xs,ys is the Sun's position in a coordinate system in the plane of the ecliptic. To convert this to equatorial, rectangular, geocentric coordinates, compute:
    float xe = xs;
    float ye = ys * cosf(ecl);
    float ze = ys * sinf(ecl);
    // Finally, compute the Sun's Right Ascension (RA) and Declination (Dec):
    RA  = atan2f( ye, xe );
    Dec = atan2f( ze, sqrt(xe*xe+ye*ye) );
}
void objPos(float d, int ob, float &x, float &y, float &z)
{
    TOrbitalElementsAt o(oe[ob], d);
    float E= (o.M+o.e)*degToRad*sinf(o.M*degToRad)*(1.0+o.e*cosf(o.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    for (int i=0; i<5; i++)
        E= E - ( E - o.e * sinf(E) - o.M*degToRad ) / ( 1 - o.e * cos(E) );
    float xv = o.a * ( cosf(E) - o.e );
    float yv = o.a * ( sqrt(1.0 - o.e*o.e) * sinf(E) );
    float v = atan2f( yv, xv );
    float r = sqrt( xv*xv + yv*yv );
    x = r * ( cosf(o.N*degToRad) * cosf(v+o.w*degToRad) - sinf(o.N*degToRad) * sinf(v+o.w*degToRad) * cosf(o.i*degToRad) );
    y = r * ( sinf(o.N*degToRad) * cosf(v+o.w*degToRad) + cosf(o.N*degToRad) * sinf(v+o.w*degToRad) * cosf(o.i*degToRad) );
    z = r * ( sinf(v+o.w*degToRad) * sin(o.i*degToRad) );
}
//    Related orbital elements are:
//
//  w1 = N + w   = longitude of perihelion
//  L  = M + w1  = mean longitude
//  q  = a*(1-e) = perihelion distance
//  Q  = a*(1+e) = aphelion distance
//  P  = a ^ 1.5 = orbital period (years if a is in AU, astronomical units)
//  T  = Epoch_of_M - (M(deg)/360_deg) / P  = time of perihelion
//  v  = true anomaly (angle between position and perihelion)
//  E  = eccentric anomaly
//  AU=149.6 million km

