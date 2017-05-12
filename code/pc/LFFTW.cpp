#include "LFFTW.h"
#include <functional>

#define HASFFTW

#ifdef HASFFTW

#include <fftw3.h>
using namespace cv;
using namespace std;

namespace llfftw
{

void fft3D(VMat & volume, VMat & realOut, VMat & imagOut)
{
    realOut = VMat(volume.s);
    imagOut = VMat(volume.s);

    fftw_complex * in   = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * volume.s3);
    fftw_complex * out  = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * volume.s3);

    for(int i = 0; i < volume.s3; i++)
    {
        in[i][0] = volume.data[i];
        in[i][1] = 0.0f;
    }

    fftw_plan plan = fftw_plan_dft_3d(volume.s, volume.s, volume.s, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    fftw_execute(plan);

    for(int i = 0; i < volume.s3; i++)
    {
        realOut.data[i] = out[i][0];
        imagOut.data[i] = out[i][1];
    }

    fftw_free(in);
    fftw_free(out);
}

void ifft3D(VMat & output, VMat & real, VMat & imag)
{
    output = VMat(real.s);

    fftw_complex * in   = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * real.s3);
    fftw_complex * out  = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * real.s3);

    for(int i = 0; i < output.s3; i++)
    {
        in[i][0] = real.data[i];
        in[i][1] = imag.data[i];
    }

    fftw_plan plan = fftw_plan_dft_3d(real.s, real.s, real.s, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);

    fftw_execute(plan);

    for(int i = 0; i < output.s3; i++)
    {
        output.data[i] = out[i][0];
    }

    fftw_free(in);
    fftw_free(out);
}

void multiplySpectrums(VMat & re1, VMat & im1, VMat & re2, VMat & im2)
{
    for(int i = 0; i < re1.s3; i++)
    {
        float R1 =  re1.data[i];
        float I1 =  im1.data[i];
        float R2 =  re2.data[i];
        float I2 = -im2.data[i];

        float TR = R1*R2 - I1*I2;
        float TI = I1*R2 + R1*I2;


        float mag = sqrt(TR*TR + TI*TI);

        re1.data[i] = TR / mag;
        im1.data[i] = TI / mag;
    }
}


Point3i phaseCorrelate(VMat & _v1, VMat & _v2)
{
    //fft3d both
    fftw_complex * v1   = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * _v1.s3);
    fftw_complex * v2   = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * _v2.s3);
    fftw_complex * f1  = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * _v1.s3);
    fftw_complex * f2  = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * _v2.s3);

    //copy data in
    for(int i = 0; i < _v1.s3; i++)
    {
        v1[i][0] = _v1.data[i]; v1[i][1] = 0.0f;
        v2[i][0] = _v2.data[i]; v2[i][1] = 0.0f;
    }

    //create plans
    fftw_plan p1 = fftw_plan_dft_3d(_v1.s, _v1.s, _v1.s, v1, f1, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan p2 = fftw_plan_dft_3d(_v1.s, _v1.s, _v1.s, v2, f2, FFTW_FORWARD, FFTW_ESTIMATE);

    fftw_execute(p1);
    fftw_execute(p2);

    //multiply spectrums
    for(int i = 0; i < _v1.s3; i++)
    {
        float R1 =  f1[i][0];
        float I1 =  f1[i][1];
        float R2 =  f2[i][0];
        float I2 = -f2[i][1];

        float TR = R1*R2 - I1*I2;
        float TI = I1*R2 + R1*I2;


        float mag = sqrt(TR*TR + TI*TI);

        f1[i][0] = TR / mag;
        f1[i][1] = TI / mag;
    }

    //invert back to spacial domain
    fftw_plan p3 = fftw_plan_dft_3d(_v1.s, _v1.s, _v1.s, f1, v1, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(p3);

    //find real peak
    int peak_location = 0;
    float peak_value = v1[0][0];
    float tmp;
    for(int i = 1; i < _v1.s3; i++)
    {
        tmp = v1[i][0];
        if(tmp > peak_value)
        {
            peak_value = tmp;
            peak_location = i;
        }
    }

    //free data
    fftw_free(v1);
    fftw_free(v2);
    fftw_free(f1);
    fftw_free(f2);
    fftw_destroy_plan(p1);
    fftw_destroy_plan(p2);
    fftw_destroy_plan(p3);

    //return the peak location
    Point3i ret(0,0,0);

    ret.z = peak_location / _v1.s2;
    peak_location %= _v1.s2;
    ret.y = peak_location / _v1.s;
    ret.x = peak_location % _v1.s;

    return VMat::filter_pc(ret, _v1.s);

}



void phaseCorrelate_rst(VMat & vol1, VMat & vol2, float & rotation, float & scale, Point3i & translation, bool hanning_window_on)
{
    //helping functions
    int hw = vol1.s / 2;
    float hw_dist = sqrt((float)(hw * hw));
    function<float(int,int,int,int)> hanningWindowScalar = [&hw,&hw_dist](int x, int y, int z, int N) -> float {
        float dist = sqrt((float)((x-hw)*(x-hw) + (y-hw)*(y-hw) + (z-hw)*(z-hw)));
        dist = hw_dist - dist;
        hw_dist *= 2.0f;
        return (0.5f * (1.0f - cos((2.0f * M_PI * dist) / (hw_dist - 1.0f))));
    };

    //end


    int S = vol1.s3;

    //generate each complex volume : use hanning window if required
    fftw_complex * v1   = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * vol1.s3);
    fftw_complex * v2   = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * vol1.s3);
    for(int z = 0, i = 0; z < vol1.s; z++)
    {
        for(int y = 0; y < vol1.s; y++)
        {
            for(int x = 0; x < vol1.s; x++, i++)
            {
                float scalar = 1.0f;
                if(hanning_window_on) scalar = hanningWindowScalar(x,y,z,vol1.s);
                v1[i][0] = vol1.data[i] * scalar;
                v2[i][0] = vol2.data[i] * scalar;
                v1[i][1] = v2[i][1] = 0.0f;
            }
        }
    }

    //create two outputs for fft data
    //fft both
    //compute magnitudes and swap quads for both, put into two volumes
    //log the volumes
    //log-polar the volumes
    //copy them back into complex volumes
    //do fft and pc on those complex volumes
    //then use PC on both too



    /*
    hanning window? cpl -> cpl
    fft BOTH cpl -> cpl
    magnitude swap quads cpl -> float
    log float -> float
    logpolar float -> dif VMatf
    PC both -> use pcfunction created
    get r and s,
    transform vol1 by that
    pc both to find T
    */
    /*
    if(!m.new_<cufftComplex>("vol1_", S, vol1_.d)) throw m.error();
    if(!m.new_<cufftComplex>("vol2_", S, vol2_.d)) throw m.error();
    if(!m.new_<float>("tmp1", S)) throw m.error();
    if(!m.new_<float>("tmp2", S)) throw m.error();
    if(!m.new_<float>("matrix", 16)) throw m.error();


    //setup different types of GPU runs
    dim3 threadsPerBlock(8, 8, 8);
    dim3 numBlocks(vol1.s / threadsPerBlock.x, vol1.s / threadsPerBlock.y, vol1.s / threadsPerBlock.z);
    int hw = vol1.s/2;
    dim3 numBlocks4M(vol1.s / threadsPerBlock.x, vol1.s / threadsPerBlock.y, hw / threadsPerBlock.z);

    //hanning window
    if(hanning_window_on)
    {
        //hanning window
        hanning_gpu<<<numBlocks, threadsPerBlock>>>(m.at<cufftComplex>("vol1_"), vol1.s);
        hanning_gpu<<<numBlocks, threadsPerBlock>>>(m.at<cufftComplex>("vol2_"), vol1.s);
        if(!m.sync()) throw m.error();
    }

    //setup fft plan
    cufftHandle plan;
    cufftPlan3d(&plan, vol1.s, vol1.s, vol1.s, CUFFT_C2C);

    //do fft on vol1_ and vol2_
    cufftExecC2C(plan, m.at<cufftComplex>("vol1_"), m.at<cufftComplex>("vol1_"), CUFFT_FORWARD);
    cufftExecC2C(plan, m.at<cufftComplex>("vol2_"), m.at<cufftComplex>("vol2_"), CUFFT_FORWARD);
    if(!m.sync()) throw m.error();

    //get the magnitude of both
    gpu_get_magnitude_swapQuads<<<numBlocks4M, threadsPerBlock>>>(m.at<cufftComplex>("vol1_"), m.at<float>("tmp1"), vol1.s);
    gpu_get_magnitude_swapQuads<<<numBlocks4M, threadsPerBlock>>>(m.at<cufftComplex>("vol2_"), m.at<float>("tmp2"), vol1.s);
    if(!m.sync()) throw m.error();

    //get the log of both
    log_on_gpu<<<numBlocks, threadsPerBlock>>>(m.at<float>("tmp1"), vol1.s);
    log_on_gpu<<<numBlocks, threadsPerBlock>>>(m.at<float>("tmp2"), vol1.s);
    if(!m.sync()) throw m.error();

    //get the log polar of tmp1 and tmp2 as tmp3 and tmp4 respectfully
    logpolar3d_gpu_complex_out<<<numBlocks, threadsPerBlock>>>(m.at<float>("tmp1"), m.at<cufftComplex>("vol1_"), vol1.s);
    logpolar3d_gpu_complex_out<<<numBlocks, threadsPerBlock>>>(m.at<float>("tmp2"), m.at<cufftComplex>("vol2_"), vol1.s);
    if(!m.sync()) throw m.error();


    //gpu_only_pc
    function<Point3i(LCuda_Host_Manager*,cufftHandle*,string,string,VMatCufftComplex*)> f =
    [](LCuda_Host_Manager * m, cufftHandle * plan, string data1, string data2, VMatCufftComplex * cpu_a) -> Point3i
    {
        int S = cpu_a->s3;
        cufftExecC2C(*plan, m->at<cufftComplex>(data1), m->at<cufftComplex>(data1), CUFFT_FORWARD);
        cufftExecC2C(*plan, m->at<cufftComplex>(data2), m->at<cufftComplex>(data2), CUFFT_FORWARD);
        if(!m->sync()) throw m->error();

        dim3 threadsPerBlock(8, 8, 8);
        dim3 numBlocks(cpu_a->s / threadsPerBlock.x, cpu_a->s / threadsPerBlock.y, cpu_a->s / threadsPerBlock.z);
        multiply_spectrums<<<numBlocks, threadsPerBlock>>>(m->at<cufftComplex>(data1), m->at<cufftComplex>(data2), cpu_a->s);
        if(!m->sync()) throw m->error();

        cufftExecC2C(*plan, m->at<cufftComplex>(data1), m->at<cufftComplex>(data1), CUFFT_INVERSE);

        if(!m->collect<cufftComplex>(data1, S, cpu_a->d)) throw m->error();
        return cpu_a->filter_phase_peak(cpu_a->peak_real(), cpu_a->s);
    };

    //end here

    //Phase Correlate
    translation = f(&m, &plan, "vol1_", "vol2_", &vol1_);
    if(!m.sync()) throw m.error();
    VMatCufftComplex::phase_correlate_rst_adjust_rs(translation, rotation, scale, vol1.s);

    //copy from vol1 to tmp1, and from vol2_ to "vol2_"
    if(!m.upload<float>("tmp1", S, vol1.data)) throw m.error();
    if(!m.upload<cufftComplex>("vol2_", S, vol2_.d)) throw m.error();

    //transform tmp1 by R/S and set into tmp2
    Mat transformation_matrix = VMat::transformation_matrix(vol1.s, 0.0f, rotation, 0.0f, scale, 0.0f, 0.0f, 0.0f);
    transformation_matrix = transformation_matrix.inv();
    if(!m.upload<float>("matrix", 16, (float*)transformation_matrix.data)) throw m.error();
    volume_transform<<<numBlocks, threadsPerBlock>>>(m.at<float>("tmp1"), m.at<float>("tmp2"), m.at<float>("matrix"), vol1.s);
    if(!m.sync()) throw m.error();

    //copy from tmp2 to vol1_
    //dim3 threadsPerBlock11(9, 9, 9);
    copytoCufftComplex<<<numBlocks, threadsPerBlock>>>(m.at<cufftComplex>("vol1_"), m.at<float>("tmp2"), vol1.s);//wastmp2
    if(!m.sync()) throw m.error();

    if(!m.upload<cufftComplex>("vol2_", S, vol2_.d)) throw m.error();

    //final PC
    translation = f(&m, &plan, "vol1_", "vol2_", &vol1_);
    if(!m.sync()) throw m.error();

    //destroy plan
    cufftDestroy(plan);
    */

    fftw_free(v1);
    fftw_free(v2);
}



}

#endif
