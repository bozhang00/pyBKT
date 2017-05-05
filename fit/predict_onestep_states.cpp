#define NPY_NO_DEPRECATED_API NPY_1_11_API_VERSION

#include <iostream>
#include <stdint.h>
#include <alloca.h>
#include <Eigen/Core>
#include <boost/python.hpp>
#include <boost/python/numeric.hpp>
#include <boost/python/ptr.hpp>
#include <Python.h>
#include <numpy/ndarrayobject.h>

using namespace Eigen;
using namespace std;
using namespace boost::python;

#if PY_VERSION_HEX >= 0x03000000
void *
#else
void
#endif
init_numpy(){
    //Py_Initialize;
    import_array();
}

//original comment:
//"TODO if we aren't outputting gamma, don't need to write it to memory (just
//need t and t+1), so we can save the stack array for each HMM at the cost of
//a branch"

/*struct double_to_python_float
{
    static PyObject* convert(double const& d)
      {
        return boost::python::incref(
          boost::python::object(d).ptr());
      }
};*/

//numpy scalar converters.
template <typename T, NPY_TYPES NumPyScalarType>
struct enable_numpy_scalar_converter
{
  enable_numpy_scalar_converter()
  {
    // Required NumPy call in order to use the NumPy C API within another
    // extension module.
    // import_array();
    init_numpy();

    boost::python::converter::registry::push_back(
      &convertible,
      &construct,
      boost::python::type_id<T>());
  }

  static void* convertible(PyObject* object)
  {
    // The object is convertible if all of the following are true:
    // - is a valid object.
    // - is a numpy array scalar.
    // - its descriptor type matches the type for this converter.
    return (
      object &&                                                    // Valid
      PyArray_CheckScalar(object) &&                               // Scalar
      PyArray_DescrFromScalar(object)->type_num == NumPyScalarType // Match
    )
      ? object // The Python object can be converted.
      : NULL;
  }

  static void construct(
    PyObject* object,
    boost::python::converter::rvalue_from_python_stage1_data* data)
  {
    // Obtain a handle to the memory block that the converter has allocated
    // for the C++ type.
    namespace python = boost::python;
    typedef python::converter::rvalue_from_python_storage<T> storage_type;
    void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;

    // Extract the array scalar type directly into the storage.
    PyArray_ScalarAsCtype(object, storage);

    // Set convertible to indicate success.
    data->convertible = storage;
  }
};

// TODO openmp version

dict run(dict& data, dict& model, numeric::array& forward_messages){
    //TODO: check if parameters are null.
    //TODO: check that dicts have the required members.
    //TODO: check that all parameters have the right sizes.
    //TODO: i'm not sending any error messages.

    numeric::array alldata = extract<numeric::array>(data["data"]); //multidimensional array, so i need to keep extracting arrays.
    int bigT = len(alldata[0]); //this should be the number of columns in the alldata object. i'm assuming is 2d array.
    int num_subparts = len(alldata);

    numeric::array allresources = extract<numeric::array>(data["resources"]);

    numeric::array starts = extract<numeric::array>(data["starts"]);
    int num_sequences = len(starts);

    numeric::array lengths = extract<numeric::array>(data["lengths"]);

    numeric::array learns = extract<numeric::array>(model["learns"]);
    int num_resources = len(learns);

    numeric::array forgets = extract<numeric::array>(model["forgets"]);

    numeric::array guesses = extract<numeric::array>(model["guesses"]);

    numeric::array slips = extract<numeric::array>(model["slips"]);

    double prior = extract<double>(model["prior"]);

    Array2d initial_distn;
    initial_distn << 1-prior, prior;

    MatrixXd As(2,2*num_resources);
    for (int n=0; n<num_resources; n++) {
        double learn = extract<double>(learns[n]);
        double forget = extract<double>(forgets[n]);
        As.col(2*n) << 1-learn, learn;
        As.col(2*n+1) << forget, 1-forget;
    }

    // forward messages
    double *all_forward_messages = mxGetPr(prhs[2]);

    //// outputs

    // lhs outputs
    if (nlhs != 1) { mexErrMsgTxt("must have one output\n"); }
    plhs[0] = mxCreateDoubleMatrix(2,bigT,mxREAL);
    double *all_predictions = mxGetPr(plhs[0]);
    Map<Array2Xd,Aligned> predictions(mxGetPr(plhs[0]),2,bigT);

    /* COMPUTATION */

    for (int sequence_index=0; sequence_index < num_sequences; sequence_index++) {
        // NOTE: -1 because Matlab indexing starts at 1
        int sequence_start = ((int) starts[sequence_index]) - 1;
        int T = (int) lengths[sequence_index];

        int16_t *resources = allresources + sequence_start;
        Map<MatrixXd> forward_messages(all_forward_messages + 2*sequence_start,2,T);
        Map<MatrixXd> predictions(all_predictions + 2*sequence_start,2,T);

        predictions.col(0) = initial_distn;
        for (int t=0; t<T-1; t++) {
            predictions.col(t+1) = As.block(0,2*(resources[t]-1),2,2) * forward_messages.col(t);
        }
    }
}

BOOST_PYTHON_MODULE(E_step){
    //import_array();
    init_numpy();
    numeric::array::set_module_and_type("numpy", "ndarray");
    //to_python_converter<double, double_to_python_float>();
    enable_numpy_scalar_converter<boost::int8_t, NPY_INT8>();
    enable_numpy_scalar_converter<boost::int16_t, NPY_INT16>();
    enable_numpy_scalar_converter<boost::int32_t, NPY_INT32>();
    enable_numpy_scalar_converter<boost::int64_t, NPY_INT64>();

    def("run", run);

}
