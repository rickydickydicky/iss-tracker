// Near-earth SGP4, ported from the reference implementation by David Vallado
// (as distributed in python-sgp4, propagation.py: _initl / sgp4init / sgp4).
// Variable names follow the reference code so the two can be compared
// line by line. Deep-space (SDP4) branches are intentionally omitted.

#include "orbit_sgp4.h"

#include <math.h>

namespace orbit {

namespace {

const double kPi    = 3.14159265358979323846;
const double kTwoPi = 2.0 * kPi;
const double kX2o3  = 2.0 / 3.0;
const double kTemp4 = 1.5e-12;

// WGS-72 (python-sgp4 'wgs72')
const double kRadiusEarthKm = wgs72::kRadiusKm;
const double kJ2   = 0.001082616;
const double kJ3   = -0.00000253881;
const double kJ4   = -0.00000165597;
const double kJ3oJ2 = kJ3 / kJ2;

inline double xke() {
    // 60 / sqrt(R^3 / mu)   [1/min in earth-radii units]
    static const double v =
        60.0 / sqrt(kRadiusEarthKm * kRadiusEarthKm * kRadiusEarthKm / wgs72::kMu);
    return v;
}

// Python-style modulo (result has the sign of the divisor)
inline double pmod(double a, double m) {
    double r = fmod(a, m);
    if (r < 0.0) r += m;
    return r;
}

}  // namespace

Sgp4Error sgp4Init(const Tle& tle, Sgp4& s) {
    const double XKE = xke();

    s.epochJd = tle.epochJd;
    s.bstar = tle.bstar;
    s.ecco  = tle.ecco;
    s.inclo = tle.inclo;
    s.nodeo = tle.nodeo;
    s.argpo = tle.argpo;
    s.mo    = tle.mo;

    s.isimp = 0;
    s.aycof = s.con41 = s.cc1 = s.cc4 = s.cc5 = s.d2 = s.d3 = s.d4 = 0.0;
    s.delmo = s.eta = s.argpdot = s.omgcof = s.sinmao = 0.0;
    s.t2cof = s.t3cof = s.t4cof = s.t5cof = s.x1mth2 = s.x7thm1 = 0.0;
    s.mdot = s.nodedot = s.xlcof = s.xmcof = s.nodecf = 0.0;

    const double ss         = 78.0 / kRadiusEarthKm + 1.0;
    const double qzms2ttemp = (120.0 - 78.0) / kRadiusEarthKm;
    const double qzms2t     = qzms2ttemp * qzms2ttemp * qzms2ttemp * qzms2ttemp;

    // ---------------- _initl ----------------
    const double ecco   = s.ecco;
    const double eccsq  = ecco * ecco;
    const double omeosq = 1.0 - eccsq;
    const double rteosq = sqrt(omeosq);
    const double cosio  = cos(s.inclo);
    const double cosio2 = cosio * cosio;

    const double ak   = pow(XKE / tle.noKozai, kX2o3);
    const double d1   = 0.75 * kJ2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
    double del_       = d1 / (ak * ak);
    const double adel = ak * (1.0 - del_ * del_ - del_ *
                        (1.0 / 3.0 + 134.0 * del_ * del_ / 81.0));
    del_              = d1 / (adel * adel);
    const double no   = tle.noKozai / (1.0 + del_);
    s.noUnkozai       = no;

    const double ao    = pow(XKE / no, kX2o3);
    const double sinio = sin(s.inclo);
    const double po    = ao * omeosq;
    const double con42 = 1.0 - 5.0 * cosio2;
    s.con41            = -con42 - cosio2 - cosio2;
    const double posq  = po * po;
    const double rp    = ao * (1.0 - ecco);
    // (gsto is not needed for near-earth propagation)

    if (no <= 0.0) return SGP4_ERR_MEAN_MOTION;
    if (kTwoPi / no >= 225.0) return SGP4_ERR_DEEP_SPACE;

    // ---------------- sgp4init (near earth) ----------------
    if (rp < 220.0 / kRadiusEarthKm + 1.0) s.isimp = 1;

    double sfour  = ss;
    double qzms24 = qzms2t;
    const double perige = (rp - 1.0) * kRadiusEarthKm;
    if (perige < 156.0) {
        sfour = perige - 78.0;
        if (perige < 98.0) sfour = 20.0;
        const double qzms24temp = (120.0 - sfour) / kRadiusEarthKm;
        qzms24 = qzms24temp * qzms24temp * qzms24temp * qzms24temp;
        sfour  = sfour / kRadiusEarthKm + 1.0;
    }
    const double pinvsq = 1.0 / posq;

    const double tsi   = 1.0 / (ao - sfour);
    s.eta              = ao * ecco * tsi;
    const double etasq = s.eta * s.eta;
    const double eeta  = ecco * s.eta;
    const double psisq = fabs(1.0 - etasq);
    const double coef  = qzms24 * pow(tsi, 4.0);
    const double coef1 = coef / pow(psisq, 3.5);
    const double cc2   = coef1 * no * (ao * (1.0 + 1.5 * etasq + eeta *
                         (4.0 + etasq)) + 0.375 * kJ2 * tsi / psisq * s.con41 *
                         (8.0 + 3.0 * etasq * (8.0 + etasq)));
    s.cc1 = s.bstar * cc2;
    double cc3 = 0.0;
    if (ecco > 1.0e-4) cc3 = -2.0 * coef * tsi * kJ3oJ2 * no * sinio / ecco;
    s.x1mth2 = 1.0 - cosio2;
    s.cc4 = 2.0 * no * coef1 * ao * omeosq *
            (s.eta * (2.0 + 0.5 * etasq) + ecco *
            (0.5 + 2.0 * etasq) - kJ2 * tsi / (ao * psisq) *
            (-3.0 * s.con41 * (1.0 - 2.0 * eeta + etasq *
            (1.5 - 0.5 * eeta)) + 0.75 * s.x1mth2 *
            (2.0 * etasq - eeta * (1.0 + etasq)) * cos(2.0 * s.argpo)));
    s.cc5 = 2.0 * coef1 * ao * omeosq * (1.0 + 2.75 *
            (etasq + eeta) + eeta * etasq);

    const double cosio4 = cosio2 * cosio2;
    const double temp1  = 1.5 * kJ2 * pinvsq * no;
    const double temp2  = 0.5 * temp1 * kJ2 * pinvsq;
    const double temp3  = -0.46875 * kJ4 * pinvsq * pinvsq * no;
    s.mdot    = no + 0.5 * temp1 * rteosq * s.con41 + 0.0625 *
                temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
    s.argpdot = (-0.5 * temp1 * con42 + 0.0625 * temp2 *
                (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
                temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4));
    const double xhdot1 = -temp1 * cosio;
    s.nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) +
                2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
    s.omgcof  = s.bstar * cc3 * cos(s.argpo);
    s.xmcof   = 0.0;
    if (ecco > 1.0e-4) s.xmcof = -kX2o3 * coef * s.bstar / eeta;
    s.nodecf  = 3.5 * omeosq * xhdot1 * s.cc1;
    s.t2cof   = 1.5 * s.cc1;
    if (fabs(cosio + 1.0) > 1.5e-12)
        s.xlcof = -0.25 * kJ3oJ2 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
    else
        s.xlcof = -0.25 * kJ3oJ2 * sinio * (3.0 + 5.0 * cosio) / kTemp4;
    s.aycof   = -0.5 * kJ3oJ2 * sinio;
    const double delmotemp = 1.0 + s.eta * cos(s.mo);
    s.delmo   = delmotemp * delmotemp * delmotemp;
    s.sinmao  = sin(s.mo);
    s.x7thm1  = 7.0 * cosio2 - 1.0;

    if (s.isimp != 1) {
        const double cc1sq = s.cc1 * s.cc1;
        s.d2 = 4.0 * ao * tsi * cc1sq;
        const double temp = s.d2 * tsi * s.cc1 / 3.0;
        s.d3 = (17.0 * ao + sfour) * temp;
        s.d4 = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * sfour) * s.cc1;
        s.t3cof = s.d2 + 2.0 * cc1sq;
        s.t4cof = 0.25 * (3.0 * s.d3 + s.cc1 * (12.0 * s.d2 + 10.0 * cc1sq));
        s.t5cof = 0.2 * (3.0 * s.d4 + 12.0 * s.cc1 * s.d3 +
                  6.0 * s.d2 * s.d2 + 15.0 * cc1sq * (2.0 * s.d2 + cc1sq));
    }

    // The reference implementation propagates to t=0 to validate the state.
    double r[3], v[3];
    return sgp4Propagate(s, 0.0, r, v);
}

Sgp4Error sgp4Propagate(const Sgp4& s, double t, double r[3], double v[3]) {
    const double XKE = xke();
    const double vkmpersec = kRadiusEarthKm * XKE / 60.0;

    // ------- update for secular gravity and atmospheric drag -------
    const double xmdf   = s.mo + s.mdot * t;
    const double argpdf = s.argpo + s.argpdot * t;
    const double nodedf = s.nodeo + s.nodedot * t;
    double argpm = argpdf;
    double mm    = xmdf;
    const double t2 = t * t;
    double nodem = nodedf + s.nodecf * t2;
    double tempa = 1.0 - s.cc1 * t;
    double tempe = s.bstar * s.cc4 * t;
    double templ = s.t2cof * t2;

    if (s.isimp != 1) {
        const double delomg   = s.omgcof * t;
        const double delmtemp = 1.0 + s.eta * cos(xmdf);
        const double delm     = s.xmcof * (delmtemp * delmtemp * delmtemp - s.delmo);
        const double temp     = delomg + delm;
        mm    = xmdf + temp;
        argpm = argpdf - temp;
        const double t3 = t2 * t;
        const double t4 = t3 * t;
        tempa = tempa - s.d2 * t2 - s.d3 * t3 - s.d4 * t4;
        tempe = tempe + s.bstar * s.cc5 * (sin(mm) - s.sinmao);
        templ = templ + s.t3cof * t3 + t4 * (s.t4cof + t * s.t5cof);
    }

    double nm = s.noUnkozai;
    double em = s.ecco;
    const double inclm = s.inclo;

    if (nm <= 0.0) return SGP4_ERR_MEAN_MOTION;

    const double am = pow(XKE / nm, kX2o3) * tempa * tempa;
    nm = XKE / pow(am, 1.5);
    em = em - tempe;
    if (em >= 1.0 || em < -0.001) return SGP4_ERR_ECCENTRICITY;
    if (em < 1.0e-6) em = 1.0e-6;
    mm = mm + s.noUnkozai * templ;
    double xlm = mm + argpm + nodem;

    nodem = fmod(nodem, kTwoPi);            // python: sign-preserving
    argpm = pmod(argpm, kTwoPi);
    xlm   = pmod(xlm, kTwoPi);
    mm    = pmod(xlm - argpm - nodem, kTwoPi);

    const double sinim = sin(inclm);
    const double cosim = cos(inclm);

    const double ep    = em;
    const double xincp = inclm;
    const double argpp = argpm;
    const double nodep = nodem;
    const double mp    = mm;
    const double sinip = sinim;
    const double cosip = cosim;

    // -------------------- long period periodics ------------------
    const double axnl = ep * cos(argpp);
    double temp = 1.0 / (am * (1.0 - ep * ep));
    const double aynl = ep * sin(argpp) + temp * s.aycof;
    const double xl   = mp + argpp + nodep + temp * s.xlcof * axnl;

    // --------------------- solve kepler's equation ---------------
    const double u = pmod(xl - nodep, kTwoPi);
    double eo1 = u;
    double tem5 = 9999.9;
    int ktr = 1;
    double sineo1 = 0.0, coseo1 = 0.0;
    while (fabs(tem5) >= 1.0e-12 && ktr <= 10) {
        sineo1 = sin(eo1);
        coseo1 = cos(eo1);
        tem5   = 1.0 - coseo1 * axnl - sineo1 * aynl;
        tem5   = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
        if (fabs(tem5) >= 0.95) tem5 = tem5 > 0.0 ? 0.95 : -0.95;
        eo1 = eo1 + tem5;
        ktr++;
    }

    // ------------- short period preliminary quantities -----------
    const double ecose = axnl * coseo1 + aynl * sineo1;
    const double esine = axnl * sineo1 - aynl * coseo1;
    const double el2   = axnl * axnl + aynl * aynl;
    const double pl    = am * (1.0 - el2);
    if (pl < 0.0) return SGP4_ERR_SEMILATUS;

    const double rl     = am * (1.0 - ecose);
    const double rdotl  = sqrt(am) * esine / rl;
    const double rvdotl = sqrt(pl) / rl;
    const double betal  = sqrt(1.0 - el2);
    temp = esine / (1.0 + betal);
    const double sinu = am / rl * (sineo1 - aynl - axnl * temp);
    const double cosu = am / rl * (coseo1 - axnl + aynl * temp);
    double su = atan2(sinu, cosu);
    const double sin2u = (cosu + cosu) * sinu;
    const double cos2u = 1.0 - 2.0 * sinu * sinu;
    temp = 1.0 / pl;
    const double temp1 = 0.5 * kJ2 * temp;
    const double temp2 = temp1 * temp;

    // -------------- update for short period periodics ------------
    const double mrt = rl * (1.0 - 1.5 * temp2 * betal * s.con41) +
                       0.5 * temp1 * s.x1mth2 * cos2u;
    su = su - 0.25 * temp2 * s.x7thm1 * sin2u;
    const double xnode = nodep + 1.5 * temp2 * cosip * sin2u;
    const double xinc  = xincp + 1.5 * temp2 * cosip * sinip * cos2u;
    const double mvt   = rdotl - nm * temp1 * s.x1mth2 * sin2u / XKE;
    const double rvdot = rvdotl + nm * temp1 * (s.x1mth2 * cos2u +
                         1.5 * s.con41) / XKE;

    // --------------------- orientation vectors -------------------
    const double sinsu = sin(su);
    const double cossu = cos(su);
    const double snod  = sin(xnode);
    const double cnod  = cos(xnode);
    const double sini  = sin(xinc);
    const double cosi  = cos(xinc);
    const double xmx   = -snod * cosi;
    const double xmy   =  cnod * cosi;
    const double ux    =  xmx * sinsu + cnod * cossu;
    const double uy    =  xmy * sinsu + snod * cossu;
    const double uz    =  sini * sinsu;
    const double vx    =  xmx * cossu - cnod * sinsu;
    const double vy    =  xmy * cossu - snod * sinsu;
    const double vz    =  sini * cossu;

    const double mr = mrt * kRadiusEarthKm;
    r[0] = mr * ux;
    r[1] = mr * uy;
    r[2] = mr * uz;
    v[0] = (mvt * ux + rvdot * vx) * vkmpersec;
    v[1] = (mvt * uy + rvdot * vy) * vkmpersec;
    v[2] = (mvt * uz + rvdot * vz) * vkmpersec;

    if (mrt < 1.0) return SGP4_ERR_DECAYED;
    return SGP4_OK;
}

}  // namespace orbit
