import ext_cppm;
import ext_ixx;
import ext_ccm;
import ext_cxxm;
import ext_cxxm_plus;
import ext_mpp;

int main()
{
    return fromCppm() + fromIxx() + fromCcm() + fromCxxm() + fromCxxmPlus() + fromMpp() == 6 ? 0
                                                                                             : 1;
}
