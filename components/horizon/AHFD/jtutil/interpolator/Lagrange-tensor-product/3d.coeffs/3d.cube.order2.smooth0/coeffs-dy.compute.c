fp t285;
fp t295;
fp t286;
fp t282;
fp t262;
fp t339;
fp t294;
fp t284;
fp t307;
fp t265;
fp t338;
fp t289;
fp t313;
fp t263;
fp t244;
fp t290;
fp t319;
fp t273;
fp t337;
fp t272;
fp t254;
fp t336;
fp t270;
fp t278;
fp t335;
fp t280;
fp t257;
fp t334;
fp t333;
fp t332;
fp t291;
fp t306;
fp t246;
fp t331;
fp t292;
fp t316;
fp t268;
fp t320;
fp t311;
fp t251;
fp t330;
fp t259;
fp t256;
fp t329;
fp t258;
fp t243;
fp t328;
fp t252;
fp t317;
fp t269;
fp t327;
fp t326;
fp t314;
fp t260;
fp t264;
fp t255;
fp t325;
fp t324;
fp t293;
fp t253;
fp t310;
fp t274;
fp t323;
fp t271;
fp t245;
fp t266;
fp t322;
fp t321;
fp t318;
fp t315;
fp t312;
fp t261;
fp t309;
fp t308;
fp t305;
fp t304;
fp t303;
fp t302;
fp t301;
fp t300;
fp t299;
fp t298;
fp t297;
fp t296;
fp t288;
fp t287;
fp t249;
fp t248;
      t285 = RATIONAL(1.0,4.0);
      t295 = z*z;
      t286 = RATIONAL(-1.0,4.0);
      t282 = t286*t295;
      t262 = x*t282;
      t339 = t262+t285*x;
      t294 = x*x;
      t284 = t285*t294;
      t307 = t294*t295;
      t265 = t286*t307;
      t338 = t284+t265;
      t289 = RATIONAL(1.0,2.0);
      t313 = y*t289;
      t263 = t294*t313;
      t244 = z*t263;
      t290 = RATIONAL(-1.0,2.0);
      t319 = y*t290;
      t273 = z*t319;
      t337 = t244+t273;
      t272 = z*t313;
      t254 = t294*t273;
      t336 = t272+t254;
      t270 = t285*t307;
      t278 = t286*t294;
      t335 = t270+t278;
      t280 = t285*t295;
      t257 = x*t280;
      t334 = t286*x+t257;
      t333 = t265+t280;
      t332 = t282+t270;
      t291 = RATIONAL(-1.0,8.0);
      t306 = t294*t291;
      t246 = y*t270;
      t331 = t246+t295*t306;
      t292 = RATIONAL(1.0,8.0);
      t316 = x*t292;
      t268 = z*t316;
      t320 = x*z;
      t311 = y*t320;
      t251 = t286*t311;
      t330 = t268+t251;
      t259 = t295*t316;
      t256 = y*t262;
      t329 = t259+t256;
      t258 = t291*t320;
      t243 = t285*t311;
      t328 = t258+t243;
      t252 = y*t257;
      t317 = x*t295;
      t269 = t291*t317;
      t327 = t252+t269;
      t326 = t243+t268;
      t314 = t294*z;
      t260 = t292*t314;
      t264 = z*t284;
      t255 = y*t264;
      t325 = t260+t255;
      t324 = t258+t251;
      t293 = y*t307;
      t253 = t290*t293;
      t310 = t289*t295;
      t274 = y*t310;
      t323 = t253+t274;
      t271 = z*t278;
      t245 = y*t271;
      t266 = z*t306;
      t322 = t245+t266;
      t321 = t246+t292*t307;
      t318 = -y+t290;
      t315 = t293+y;
      t312 = t289-y;
      t261 = x*t319;
      t309 = t295*t261+t253;
      t308 = x*t274+t253;
      t305 = -x+t317;
      t304 = -z+t314;
      t303 = t252+t259+t321;
      t302 = t269+t256+t321;
      t301 = t260+t245+t331;
      t300 = t285*z+t271+t323;
      t299 = t266+t255+t331;
      t298 = t263+x*t313+t309;
      t297 = t264+t286*z+t323;
      t296 = t261+t263+t308;
      t288 = RATIONAL(-2.0,1.0);
      t287 = RATIONAL(2.0,1.0);
      t249 = z*t261;
      t248 = x*t272;
      coeffs_dy->coeff_m1_m1_m1 = t301+t328+t329;
      coeffs_dy->coeff_0_m1_m1 = t300+t332+t337;
      coeffs_dy->coeff_p1_m1_m1 = t301+t327+t330;
      coeffs_dy->coeff_m1_0_m1 = t249+t244+t308;
      coeffs_dy->coeff_0_0_m1 = t293+(-t295-t304)*y;
      coeffs_dy->coeff_p1_0_m1 = t248+t244+t309;
      coeffs_dy->coeff_m1_p1_m1 = t302+t322+t326;
      coeffs_dy->coeff_0_p1_m1 = t297+t333+t337;
      coeffs_dy->coeff_p1_p1_m1 = t303+t322+t324;
      coeffs_dy->coeff_m1_m1_0 = t296+t335+t339;
      coeffs_dy->coeff_0_m1_0 = t290+t312*t295+(t290*t295+t312)*t294+t315;
      coeffs_dy->coeff_p1_m1_0 = t298+t334+t335;
      coeffs_dy->coeff_m1_0_0 = t293+(-t294-t305)*y;
      coeffs_dy->coeff_0_0_0 = (t287*t295+t288+(t288*t295+t287)*t294)*y;
      coeffs_dy->coeff_p1_0_0 = t293+(-t294+t305)*y;
      coeffs_dy->coeff_m1_p1_0 = t296+t334+t338;
      coeffs_dy->coeff_0_p1_0 = t289+t318*t295+(t310+t318)*t294+t315;
      coeffs_dy->coeff_p1_p1_0 = t298+t338+t339;
      coeffs_dy->coeff_m1_m1_p1 = t299+t329+t330;
      coeffs_dy->coeff_0_m1_p1 = t297+t332+t336;
      coeffs_dy->coeff_p1_m1_p1 = t299+t327+t328;
      coeffs_dy->coeff_m1_0_p1 = t254+t248+t308;
      coeffs_dy->coeff_0_0_p1 = t293+(-t295+t304)*y;
      coeffs_dy->coeff_p1_0_p1 = t254+t249+t309;
      coeffs_dy->coeff_m1_p1_p1 = t302+t324+t325;
      coeffs_dy->coeff_0_p1_p1 = t300+t333+t336;
      coeffs_dy->coeff_p1_p1_p1 = t303+t325+t326;
