fp t786;
fp t808;
fp t787;
fp t807;
fp t790;
fp t806;
fp t783;
fp t805;
fp t782;
fp t804;
fp t789;
fp t788;
fp t803;
fp t793;
fp t776;
fp t802;
fp t801;
fp t800;
fp t794;
fp t771;
fp t799;
fp t798;
fp t797;
fp t781;
fp t796;
fp t780;
fp t795;
fp t792;
fp t791;
fp t778;
fp t775;
fp t774;
fp t772;
fp t770;
fp t769;
fp t768;
fp t767;
fp t765;
fp t764;
fp t763;
fp t762;
fp t761;
fp t760;
fp t759;
fp t758;
fp t757;
      t786 = RATIONAL(1.0,2.0);
      t808 = t786*y;
      t787 = RATIONAL(-1.0,2.0);
      t807 = t787*x;
      t790 = y*y;
      t806 = t790*x;
      t783 = RATIONAL(-1.0,4.0);
      t805 = y*t783;
      t782 = RATIONAL(1.0,4.0);
      t804 = y*t782;
      t789 = x*x;
      t788 = t790*t789;
      t803 = -t790+t788;
      t793 = t787*t789;
      t776 = t790*t793;
      t802 = t776+y*t807;
      t801 = -t789+t788;
      t800 = t789+t790;
      t794 = t782*t790;
      t771 = t789*t794;
      t799 = t783*t806+t771;
      t798 = t771+x*t794;
      t797 = t776+x*t808;
      t781 = t786*t790;
      t796 = t776+t781;
      t780 = t786*t789;
      t795 = t776+t780;
      t792 = x-t806;
      t791 = (1.0-t789)*y;
      t778 = x*t805;
      t775 = y*t780;
      t774 = x*t804;
      t772 = t789*t805;
      t770 = x*t781;
      t769 = t789*t804;
      t768 = t787*t806;
      t767 = y*t793;
      t765 = RATIONAL(1.0,1.0)+t788-t800;
      t764 = t775+t787*y+t796;
      t763 = t770+t807+t795;
      t762 = t786*x+t768+t795;
      t761 = t808+t767+t796;
      t760 = t772+t774+t799;
      t759 = t772+t778+t798;
      t758 = t769+t778+t799;
      t757 = t769+t774+t798;
      coeffs_dzz->coeff_m1_m1_m1 = t760;
      coeffs_dzz->coeff_0_m1_m1 = t764;
      coeffs_dzz->coeff_p1_m1_m1 = t759;
      coeffs_dzz->coeff_m1_0_m1 = t763;
      coeffs_dzz->coeff_0_0_m1 = t765;
      coeffs_dzz->coeff_p1_0_m1 = t762;
      coeffs_dzz->coeff_m1_p1_m1 = t758;
      coeffs_dzz->coeff_0_p1_m1 = t761;
      coeffs_dzz->coeff_p1_p1_m1 = t757;
      coeffs_dzz->coeff_m1_m1_0 = t775+t770+t802;
      coeffs_dzz->coeff_0_m1_0 = t791+t803;
      coeffs_dzz->coeff_p1_m1_0 = t775+t768+t797;
      coeffs_dzz->coeff_m1_0_0 = t792+t801;
      coeffs_dzz->coeff_0_0_0 = (1.0+t788)*RATIONAL(-2.0,1.0)+t800*RATIONAL(2.0
,1.0);
      coeffs_dzz->coeff_p1_0_0 = -t792+t801;
      coeffs_dzz->coeff_m1_p1_0 = t770+t767+t797;
      coeffs_dzz->coeff_0_p1_0 = -t791+t803;
      coeffs_dzz->coeff_p1_p1_0 = t768+t767+t802;
      coeffs_dzz->coeff_m1_m1_p1 = t760;
      coeffs_dzz->coeff_0_m1_p1 = t764;
      coeffs_dzz->coeff_p1_m1_p1 = t759;
      coeffs_dzz->coeff_m1_0_p1 = t763;
      coeffs_dzz->coeff_0_0_p1 = t765;
      coeffs_dzz->coeff_p1_0_p1 = t762;
      coeffs_dzz->coeff_m1_p1_p1 = t758;
      coeffs_dzz->coeff_0_p1_p1 = t761;
      coeffs_dzz->coeff_p1_p1_p1 = t757;