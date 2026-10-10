// Self-contained Android ARM64 CPU worker: evaluates all real original Koikatsu
// sparse/dense POSITION morph targets for 61 frames without Windows/Unreal RAM.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

struct Base {uint32_t n;uint64_t o;uint32_t step;};
struct Morph {uint32_t group,n,spCount,indexType;uint64_t indexOff,valueOff,denseOff;uint32_t denseStride;bool sparse;};
struct Vec {float x,y,z;};
static const float PI=3.14159265358979323846f;

template<class T> T read(const std::vector<uint8_t>& b,uint64_t offset){
 if(offset+sizeof(T)>b.size())throw std::runtime_error("GLB byte offset outside file");
 T v;std::memcpy(&v,b.data()+offset,sizeof(T));return v;
}
static Vec vread(const std::vector<uint8_t>& b,uint64_t offset){
 Vec v=read<Vec>(b,offset);
 if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))
   throw std::runtime_error("Nonfinite original position delta");
 return v;
}
static uint32_t sidx(const std::vector<uint8_t>& b,const Morph& m,uint32_t i){
 switch(m.indexType){
 case 5121:return read<uint8_t>(b,m.indexOff+i);
 case 5123:return read<uint16_t>(b,m.indexOff+uint64_t(i)*2);
 case 5125:return read<uint32_t>(b,m.indexOff+uint64_t(i)*4);
 default:throw std::runtime_error("Invalid sparse index type");
 }
}
int main(int argc,char** argv){
 try{
  if(argc!=3)throw std::runtime_error("Usage: kk_morph_validate INPUT.glb META.txt");
  auto start=std::chrono::steady_clock::now();
  std::ifstream src(argv[1],std::ios::binary|std::ios::ate);
  if(!src)throw std::runtime_error("Cannot read real source GLB on phone");
  auto size=src.tellg();if(size<100)throw std::runtime_error("Invalid input bytes");
  std::vector<uint8_t> glb(size_t(size),0);src.seekg(0);src.read(reinterpret_cast<char*>(glb.data()),size);
  if(std::memcmp(glb.data(),"glTF",4)!=0)throw std::runtime_error("Not a GLB");
  std::ifstream in(argv[2]);if(!in)throw std::runtime_error("Missing workload meta");
  std::string magic;int bCount,mCount,frames;uint64_t expected;
  in>>magic>>bCount>>mCount>>frames>>expected;
  if(magic!="KKMORPH1"||bCount<1||bCount>500||mCount<1||mCount>100000
     ||frames!=61 || expected!=glb.size())throw std::runtime_error("Header mismatch");
  std::vector<Base> bases(bCount);std::vector<std::vector<Morph>> morphs(bCount);
  int baseRead=0,morphRead=0;char kind;
  while(in>>kind){
   if(kind=='B'){
    int g;Base b{};in>>g>>b.n>>b.o>>b.step;
    if(g<0||g>=bCount||b.n>1000000||b.step<12)throw std::runtime_error("Bad base");
    bases[g]=b;++baseRead;
   }else if(kind=='M'){
    Morph m{};int sparse;in>>m.group>>sparse>>m.n>>m.spCount>>m.indexType
      >>m.indexOff>>m.valueOff>>m.denseOff>>m.denseStride;
    m.sparse=(sparse!=0);
    if(m.group>=uint32_t(bCount)||m.n>1000000||m.spCount>m.n
       ||(!m.sparse&&m.denseStride<12))throw std::runtime_error("Bad morph descriptor");
    morphs[m.group].push_back(m);++morphRead;
   }else throw std::runtime_error("Bad workload line");
  }
  if(baseRead!=bCount||morphRead!=mCount)throw std::runtime_error("Mismatched workload groups");
  double accumulated=0;double maxDelta=0;
  uint64_t evaluated=0,nonZero=0,invalid=0;
  const auto compute=std::chrono::steady_clock::now();
  for(int group=0;group<bCount;++group){
   Base b=bases[group];
   std::vector<Vec> original(b.n),work(b.n);
   for(uint32_t i=0;i<b.n;++i)original[i]=vread(glb,b.o+uint64_t(i)*b.step);
   for(int f=0;f<frames;++f){
    work=original;
    for(size_t ti=0;ti<morphs[group].size();++ti){
     const Morph& m=morphs[group][ti];
     if(m.n!=b.n)throw std::runtime_error("Morph/accessor vertex count mismatch");
     float w=0.1f*std::sin(2.f*PI*float(f)/float(frames-1)
             +float((ti*11+group*13)%59)*0.08f);
     if(m.sparse){
      for(uint32_t k=0;k<m.spCount;++k){
       uint32_t idx=sidx(glb,m,k);
       if(idx>=b.n)throw std::runtime_error("Invalid sparse morph vertex index");
       Vec d=vread(glb,m.valueOff+uint64_t(k)*12);
       work[idx].x+=w*d.x;work[idx].y+=w*d.y;work[idx].z+=w*d.z; ++evaluated;
      }
     }else{
      for(uint32_t k=0;k<b.n;++k){
       Vec d=vread(glb,m.denseOff+uint64_t(k)*m.denseStride);
       work[k].x+=w*d.x;work[k].y+=w*d.y;work[k].z+=w*d.z; ++evaluated;
      }
     }
    }
    for(uint32_t i=0;i<b.n;++i){
     const Vec& v=work[i];const Vec& o=original[i];
     if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)){++invalid;continue;}
     double dx=double(v.x)-o.x,dy=double(v.y)-o.y,dz=double(v.z)-o.z;
     double dd=std::sqrt(dx*dx+dy*dy+dz*dz);
     if(dd>1e-6)++nonZero;
     accumulated+=dd;maxDelta=std::max(maxDelta,dd);
    }
   }
  }
  auto stop=std::chrono::steady_clock::now();
  auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(stop-compute).count();
  auto totalms=std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count();
  if(invalid)throw std::runtime_error("Rendered morph produced NaN or infinity");
  std::cout<<"{\"status\":\"PASS_REAL_PHONE_CPU_MORPH_EVALUATION\",\"primitives\":"
    <<bCount<<",\"source_morph_targets\":"<<mCount<<",\"frames\":"<<frames
    <<",\"executed_sparse_dense_vertex_deltas\":"<<evaluated
    <<",\"changed_vertex_samples\":"<<nonZero
    <<",\"max_morph_vertex_delta\":"<<maxDelta
    <<",\"sum_delta\":"<<accumulated
    <<",\"invalid_vertices\":"<<invalid
    <<",\"compute_elapsed_ms\":"<<ms
    <<",\"total_elapsed_ms\":"<<totalms<<"}\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"PHONE_MORPH_FAIL "<<e.what()<<"\n";return 2;}
}
