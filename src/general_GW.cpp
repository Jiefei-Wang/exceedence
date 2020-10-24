#include <Rcpp.h>
#include <map>
#include "bit_class.h"
using namespace Rcpp;

//Generate the subset index with a specific size for the samples 
class Subset_iterator{
    public:
      size_t sample_size;
      size_t subset_size;
      std::vector<size_t> index;
      Subset_iterator(size_t sample_size,size_t subset_size): sample_size(sample_size),subset_size(subset_size){
          index.resize(subset_size+1);
          for(size_t i = 0;i<subset_size;i++){
              index[i] = i;
          }
          index[subset_size] = sample_size;
      }
      bool next(){
          for(size_t k = 0;k<subset_size; k++){
              size_t i = subset_size - k -1;
              if(index[i]+1!=index[i+1]){
                  index[i] ++;
                  for(size_t j=i+1;j<subset_size-1;j++){
                    index[j] = index[j-1]+1;
                  }
                  return true;
              }
          }
          return false;
      }
};

void make_subset(NumericVector& samples, NumericVector& subset, std::vector<size_t>& index){
    double* ptr_src = (double*) DATAPTR(samples);
    double* ptr_dest = (double*) DATAPTR(subset);
    for(size_t i=0;i<index.size()-1;i++){
        ptr_dest[i] = ptr_src[index[i]];
    }
}

void subset_obj_finalizer(SEXP x) {
		delete (std::multimap<double, Bit_set_class>*)R_ExternalPtrAddr(x);
}

// [[Rcpp::export]]
SEXP general_GW_construct_subset(Function pvalue_func, NumericVector x){
    //This map stores p-value: index pair
    std::multimap<double, Bit_set_class>* candidate_subset = new std::multimap<double, Bit_set_class>();
    R_xlen_t m = XLENGTH(x);
    NumericVector subset = NumericVector(m);
    Bit_set_class subset_index(m);
    for(size_t curN =m-1;curN>0;curN--){
        Subset_iterator iterator(m, curN);
        do{
            subset_index.set_bit(iterator.index);
            //Get the subset samples
            make_subset(x,subset,iterator.index);
            double p_value = as<double>(pvalue_func(subset));
            /*
            For all the existing sets whose p-value is greater or equal than 
            the current p-value, we will check the subset relationship.
            If the current is a subset of an existing set, we will ignore it.
            Otherwise we add it to the candidate_subset.
            */
            auto lower_iter = candidate_subset->lower_bound(p_value);
            bool add_to_result = true;
            for(auto it = lower_iter; it!=candidate_subset->end();it++){
                if(it->second.contain(subset_index)){
                    add_to_result = false;
                    break;
                }
            }
            if(add_to_result){
                candidate_subset->emplace(p_value,subset_index);
            }
        }while(iterator.next());
    }
    SEXP R_ptr = Rf_protect(R_MakeExternalPtr(candidate_subset, R_NilValue, R_NilValue));
	R_RegisterCFinalizer(R_ptr, subset_obj_finalizer);
	Rf_unprotect(1);
    return R_ptr;
}


// [[Rcpp::export]]
void print_subset_list(SEXP x){
    std::multimap<double, Bit_set_class>* candidate_subset = (std::multimap<double, Bit_set_class>*)R_ExternalPtrAddr(x);
    for(auto& i: *candidate_subset){
        Rprintf("%f : %s\n", i.first, i.second.to_string().c_str());
    }
}
