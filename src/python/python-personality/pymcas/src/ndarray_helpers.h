#ifndef __NDARRAY_HELPERS_H__
#define __NDARRAY_HELPERS_H__

/** 
 * Extract NumPy ndarray metadata into byte array
 * 
 * @param self 
 * @param args 
 * @param kwargs 
 * 
 * @return Bytearray metadata header
 */
PyObject * pymcas_ndarray_header(PyObject * self,
                                 PyObject * args,
                                 PyObject * kwargs);

/** 
 * Get size of header for ndarray
 * 
 * @param self 
 * @param args 
 * @param kwargs 
 * 
 * @return Size of header in bytes
 */
PyObject * pymcas_ndarray_header_size(PyObject * self,
                                      PyObject * args,
                                      PyObject * kwargs);

/** 
 * Create an NumPy ndarray from existing memory
 * 
 * @param self 
 * @param args 
 * @param kwargs 
 * 
 * @return 
 */
PyObject * pymcas_ndarray_from_bytes(PyObject * self,
                                     PyObject * args,
                                     PyObject * kwargs);



#endif // __NDARRAY_HELPERS_H__
