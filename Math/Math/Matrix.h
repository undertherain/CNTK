//
// <copyright file="Matrix.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//

// TODO:
//  - remove empty-matrix checks: if an op is well-defined with empty matrices, then do it
//  - Resize() must be cheap if it does nothing  (I already did that for CPU, still to be done for GPU)
//  - an overload for Resize() to match another matrix
//  - need a way to grow a minibatch matrix without destroying its content, something like PushColumns()
#pragma once

#ifdef    _WIN32
#ifdef MATH_EXPORTS
#define MATH_API __declspec(dllexport)
#else
#define MATH_API __declspec(dllimport)
#endif
#else    // no DLLs on Linux
#define    MATH_API 
#endif

#include "Basics.h"
#include "File.h"
#include "CommonMatrix.h"
#include <limits.h>
#include <memory>   // for shared_ptr

// This class is exported from the Math.dll
namespace Microsoft { namespace MSR { namespace CNTK {


    enum CurrentDataLocation
    {
        NONE, CPU, GPU, BOTH
    };

    enum MatrixType
    { 
       UNDETERMINED, DENSE, SPARSE
    };

    // TODO: create an <ElemType>-agnostic base class, then move generic functions such as getting dims, resizing, and getting/setting as scalars
    class MATH_API MatrixBase
    {
    protected:
        //virtual ~MatrixBase() { };
        // TODO: currently this causes link errors when building DLLs
    };

    // avoid pulling in these header files for consumers of this class
    template<class ElemType> class GPUMatrix;
    template<class ElemType> class CPUMatrix;
    template<class ElemType> class GPUSparseMatrix;
    template<class ElemType> class CPUSparseMatrix;
    template<class ElemType> class DeviceBoundNumber;

    //To compy with BLAS libraries matrices are stored in ColMajor. However, by default C/C++/C# use RowMajor
    //convertion is need when passing data between Matrix and C++ matrices
    //For the best performance compile CNTKMath project with NO_SYNC preprocessor directive
    //!!!WARNING!!! This class is NOT THREAD SAFE. Test and add necessary modifications if using in multi-threaded environment    
    template<class ElemType>
    class MATH_API Matrix : public MatrixBase
    {
    private:
        mutable BaseMatrix<ElemType> *m_baseMatrix;
        mutable GPUMatrix<ElemType> *m_GPUMatrix;
        mutable CPUMatrix<ElemType> *m_CPUMatrix;
        mutable GPUSparseMatrix<ElemType> *m_GPUSparseMatrix;
        mutable CPUSparseMatrix<ElemType> *m_CPUSparseMatrix;
        mutable MatrixType m_matrixType;
        mutable CurrentDataLocation m_currentDataLocation; //Indicates which matrix is current        
        mutable DEVICEID_TYPE m_preferredDeviceId;

        mutable size_t m_numTimesDeviceChanged;
        mutable size_t m_numTimesMatrixTypeChanged;
        mutable int m_devicesTransferedTo[2];       // TODO: what is this for? Seems only diagnostics
            
        //Moves matrix from device id_from to device with id_to. This method doesn't change preferred device Id
        void _transferFromDeviceToDevice(int id_from, int id_to, bool ismoved=true,bool emptyTransfer=false) const; 
        //Moves matrix from current device to device with id_to. This method doesn't change preferred device Id
        void _transferToDevice(int id_to, bool ismoved=true, bool emptyTransfer=false) const; 
        static void DecideAndMoveToRightDevice(const Matrix<ElemType>& a, const Matrix<ElemType>& b);
        static void DecideAndMoveToRightDevice(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const Matrix<ElemType>& c);
        static void CopyElementsFromDenseToSparse(CPUMatrix<ElemType>& from, CPUSparseMatrix<ElemType>& dest);

    public:
        //Constructors, destructors and other static matrix builders
        //Each constructor can take deviceId as parameter.
        //If deviceId<0 then the matrix will be based in RAM (CPUMatrix)
        //Elseif deviceId>=0 and <AUTOPLACEMATRIX, then the matrix will be based on GPU with specified deviceId
        //Else (default) if deviceId=AUTOPLACEMATRIX, the class will try to place itself on the best GPU, if fails it will go to CPU
        //The default behaiviour should be deviceId=AUTOPLACEMATRIX        
        Matrix(DEVICEID_TYPE deviceId=AUTOPLACEMATRIX); 
        Matrix(BaseMatrix<ElemType>* baseMatrix, ElemType *pArray, DEVICEID_TYPE deviceId); // constructor for setting Matrix from a base matrix (externally managed butter pArray)
        Matrix(FILE* f, const char * matrixName, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX, const MatrixType matrixType = DENSE); //matrixName is used to verify that correct matrix is read.
        Matrix(const size_t numRows, const size_t numCols, DEVICEID_TYPE deviceId = AUTOPLACEMATRIX, const MatrixType matrixType = DENSE, const MatrixFormat matrixFormat = matrixFormatDense);
        Matrix(const size_t numRows, const size_t numCols, ElemType *pArray, const size_t matrixFlags=matrixFlagNormal, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX, const size_t nnz=0);
        Matrix(const Matrix<ElemType>& deepCopyFrom, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX);  //copy constructor, deep copy
        Matrix<ElemType>& operator=(const Matrix<ElemType>& deepCopyFrom);  //assignment operator, deep copy
        Matrix(Matrix<ElemType>&& moveFrom);  //move constructor, shallow copy
        Matrix<ElemType>& operator=(Matrix<ElemType>&& moveFrom);  //move coment operator, shallow copy

        static Matrix<ElemType> Ones(const size_t rows, const size_t cols, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX);
        static Matrix<ElemType> Zeros(const size_t rows, const size_t cols, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX);
        static Matrix<ElemType> Eye(const size_t rows, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX);

#define USE_TIME_BASED_SEED ULONG_MAX
        static Matrix<ElemType> RandomUniform(const size_t rows, const size_t cols, const ElemType low, const ElemType high, unsigned long seed = USE_TIME_BASED_SEED, DEVICEID_TYPE deviceId = AUTOPLACEMATRIX);
        static Matrix<ElemType> RandomGaussian(const size_t rows, const size_t cols, const ElemType mean, const ElemType sigma, unsigned long seed=USE_TIME_BASED_SEED, DEVICEID_TYPE deviceId=AUTOPLACEMATRIX);

        void Clear();
        ~Matrix();

    private:
        Matrix(const MatrixFlags matrixFlags, const MatrixType matrixType, const MatrixFormat matrixFormat, DEVICEID_TYPE deviceID); //only used internally to initialize a blank matrix
        Matrix(const MatrixFlags matrixFlags, const MatrixType matrixType, DEVICEID_TYPE deviceID); //only used internally to initialize a blank matrix
        Matrix(const MatrixFlags matrixFlags, DEVICEID_TYPE deviceID); //only used internally to initialize a blank matrix
        void Init(DEVICEID_TYPE deviceID); //only used internally to initialize a blank matrix
        void SetDataLocation(CurrentDataLocation location, MatrixType type=UNDETERMINED) const;

    public:
        MatrixType GetMatrixType() const {return m_matrixType;};
        MatrixFormat GetFormat() const { return m_baseMatrix->GetFormat(); }
        bool OwnBuffer() const {return m_baseMatrix->OwnBuffer();}
        int GetDeviceId() const; //-1 if CPU, otherwise GPU CUDA device id
        DEVICEID_TYPE GetPreferredDeviceId() const { return m_preferredDeviceId; }; //-1 if CPU, otherwise GPU CUDA device id
        void SetPreferredDeviceId(DEVICEID_TYPE preferredDeviceId){ if (m_preferredDeviceId != preferredDeviceId) m_preferredDeviceId = preferredDeviceId; }
        //Moves matrix from device id_from to device with id_to. 
        //If emptyTransfer=true, then no data is ever moved, just corresponding GPU/CPU matrices are deleted and then created using empty constructor
        void TransferFromDeviceToDevice(int id_from, int id_to, bool ismoved = false,/*if false then keep source and set location to BOTH*/ bool emptyTransfer = false, bool updatePreferredDevice = true) const;
        //Same as TransferFromDeviceToDevice() but moves only if it is currently not on the target device
        void TransferToDeviceIfNotThere(int id_to, bool ismoved = false, bool emptyTransfer = false, bool updatePreferredDevice = true) const;
        void TransferToDeviceIfNotThereAndNotAutoPlace(int id_to, bool ismoved = false, bool emptyTransfer = false, bool updatePreferredDevice = true) const;
        CurrentDataLocation GetCurrentMatrixLocation() const { return m_currentDataLocation; };
        void SwitchToMatrixType(MatrixType newMatrixType, MatrixFormat newMatrixFormat, bool keepValues); //sets matrix type between dense and sparse
        size_t GetNumRows() const;
        size_t GetNumCols() const;
        size_t GetNumElements() const;
        bool HasNoElements() const { return GetNumElements() == 0; }
        wchar_t* GetMatrixName() const;
        void SetMatrixName(const wchar_t* s);
        bool IsEmpty() const;  
        size_t BufferSize() const;
        ElemType* BufferPointer() const;
        size_t NzCount() const;

        ElemType* CopyToArray() const; //allocated by the callee but need to be deleted by the caller
        size_t CopyToArray(ElemType*& arrayCopyTo, size_t& currentArraySize) const;  //allocated by the callee but need to be deleted by the caller
        // colStride specifies leading dimension of dst.
        // REVIEW alexeyk: GPU version copies from device to host only, implement all versions (device <-> host).
        void CopySection(size_t numRows, size_t numCols, ElemType* dst, size_t colStride) const; 

        Matrix<ElemType> ColumnSlice(size_t startColumn, size_t numCols) const;


        // difference between AssignColumnSlice and SetColumnSlice 
        // AssignColumnSlice :      this(:, startColumn:startColumn+numCols-1) = fromMatrix(:, startColumn: startColumn+numCols-1) 
        // SetColumnSlice    :      this(:, startColumn:startColumn+numCols-1) = fromMatrix(:, 0: startColumn+numCols-1) 
        // AssignColumnSlice do not transfer data, it uses external data
        // SetColumnSlice    copies data 

        Matrix<ElemType>& AssignColumnSlice(const Matrix<ElemType>& fromMatrix, size_t startColumn, size_t numCols);
        Matrix<ElemType>& SetColumnSlice(const Matrix<ElemType>& fromMatrix, size_t startColumn, size_t numCols);

        void CopyColumnsStrided(const Matrix<ElemType>& fromMatrix, size_t numCols, size_t srcNumColsStride, size_t destNumColsStride);

        Matrix<ElemType> Diagonal() const;
        Matrix<ElemType> AssignDiagonalValuesTo(Matrix<ElemType>& diag) const;
        void ShiftBy(int numShift);

        // TODO: all these scalars should be passed as doubles and cast down inside
        void NormalGrad(Matrix<ElemType>& gradients, Matrix<ElemType>& functionValues, const ElemType learnRatePerSample, const ElemType momentum);
        ElemType Adagrad(Matrix<ElemType>& gradients, const bool needAveMultiplier);
        void FSAdagrad(size_t mbSize, Matrix<ElemType>& gradients, Matrix<ElemType>& functionValues, const ElemType learnRatePerSample, const ElemType momentum);
        ElemType RmsProp(Matrix<ElemType>& gradients, ElemType RMS_GAMMA, ElemType RMS_WGT_INC, ElemType RMS_WGT_MAX, ElemType RMS_WGT_DEC, ElemType RMS_WGT_MIN, const bool needAveMultiplier);
       
        void Resize(const size_t numRows, const size_t numCols, const size_t numNZElemToReserve = 10000, bool growOnly = true);  //by default we only reallocate if need to grow        
        void VerifySize(size_t rows, size_t cols)
        {
            if (rows != GetNumRows() || cols != GetNumCols())
                LogicError("VerifySize: expected m_functionValues size %d x %d, but it is %d x %d",
                (int)rows, (int)cols, (int)GetNumRows(), (int)GetNumCols());
        }
        Matrix<ElemType> AsReference() { return ColumnSlice(0, GetNumCols()); } // get a reference (e.g. this is not resizable but can be reshaped)
        void Reshape(const size_t numRows, const size_t numCols);               // note: reshapes in place. To get a reshaped reference, use Reshaped()
        Matrix<ElemType> Reshaped(const size_t numRows, const size_t numCols)   // get a reshaped reference
        {
            Matrix<ElemType> result = AsReference();
            result.Reshape(numRows, numCols);
            return result;
        }

        // update number of columns
        // TODO: a future version may want to enforce retaining the content, to allow dynamically growing layouts column by column (when size is not known upfront)
        void ResizeColumns(const size_t numCols) { Resize(GetNumRows(), numCols); }

        // similarl to the repmat operation in matlab or octave
        static Matrix<ElemType> RepMat(const Matrix<ElemType>& frmMat, const size_t rows, const size_t cols);
        size_t GetAllocatedSize() const;
        void Reset(); //reset for sparse matrix

        const ElemType operator() (const size_t row, const size_t col) const;
        ElemType& operator() (const size_t row, const size_t col);
        ElemType Get00Element() const;

        void SetValue(const ElemType v);
        void SetValue(const DeviceBoundNumber<ElemType>& db_number);
        void SetValue(const Matrix<ElemType>& deepCopyFrom, const MatrixFormat format=matrixFormatSparseCSR);
        void SetValue(const size_t numRows, const size_t numCols, int deviceId, ElemType *pArray, const size_t matrixFlags = matrixFlagNormal);
        void SetValue(const size_t rIdx, const size_t cIdx, ElemType val);  // set matrix sparsely
        static ElemType MakeNan(size_t payload);
        void Invalidate() { SetValue(MakeNan(__LINE__)); }
        void SetMatrixFromCSCFormat(const CPUSPARSE_INDEX_TYPE *h_CSCCol, const CPUSPARSE_INDEX_TYPE *h_Row, const ElemType *h_Val,
            const size_t nz, const size_t numRows, const size_t numCols);

        void MaskColumnsValue(const Matrix<char>& columnsMask, ElemType val);

        void SetColumn(const ElemType* colPointer, size_t colInd);
        void SetColumn(const ElemType val, size_t colInd);
        void SetColumn(const Matrix<ElemType>& valMat, size_t colInd);

        void SetDiagonalValue(const ElemType v);
        void SetDiagonalValue(const Matrix<ElemType>& vector);
        void SetUniformRandomValue(const ElemType low, const ElemType high, unsigned long seed=USE_TIME_BASED_SEED);
        void SetGaussianRandomValue(const ElemType mean, const ElemType sigma, unsigned long seed=USE_TIME_BASED_SEED);
        void SetUniformRandomMask(const ElemType maskRate, const ElemType scaleValue, unsigned long seed=USE_TIME_BASED_SEED); 
        void AddGaussianRandomValue(const ElemType mean, const ElemType sigma, unsigned long seed=USE_TIME_BASED_SEED);
        Matrix<ElemType>& AssignNoiseContrastiveEstimation(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const Matrix<ElemType>& c, const Matrix<ElemType>& bias, Matrix<ElemType>& tmp);

        Matrix<ElemType>& AssignNCEDerivative(const Matrix<ElemType>& tmp, const Matrix<ElemType>& a, const Matrix<ElemType>& b, const Matrix<ElemType>& c, size_t inputIndex);
        Matrix<ElemType>& AssignSoftmaxSum(const Matrix<ElemType>& a, const Matrix<ElemType>& softmax);
        Matrix<ElemType>& AssignNceUnnormalizedEval(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const Matrix<ElemType>& c, const Matrix<ElemType>& bias);

        Matrix<ElemType> Transpose(); // This method doesn't change state of Matrix. It should be a const function
        Matrix<ElemType>& AssignTransposeOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& operator+= (const ElemType alpha);
        Matrix<ElemType> operator+ (const ElemType alpha) const;
        Matrix<ElemType>& AssignSumOf(const ElemType alpha, const Matrix<ElemType>& a);

        Matrix<ElemType>& operator+= (const Matrix<ElemType>& a);
        Matrix<ElemType> operator+ (const Matrix<ElemType>& a) const;
        Matrix<ElemType>& AssignSumOf(const Matrix<ElemType>& a, const Matrix<ElemType>& b);

        Matrix<ElemType>& operator-= (const ElemType alpha);
        Matrix<ElemType> operator- (const ElemType alpha) const;
        Matrix<ElemType>& AssignDifferenceOf(const ElemType alpha, const Matrix<ElemType>& a);
        Matrix<ElemType>& AssignDifferenceOf(const Matrix<ElemType>& a, const ElemType alpha);

        Matrix<ElemType>& operator-= (const Matrix<ElemType>& a);
        Matrix<ElemType> operator- (const Matrix<ElemType>& a) const;
        Matrix<ElemType>& AssignDifferenceOf(const Matrix<ElemType>& a, const Matrix<ElemType>& b);

        Matrix<ElemType>& operator*= (const ElemType alpha);
        Matrix<ElemType> operator* (const ElemType alpha) const;
        Matrix<ElemType>& AssignProductOf(const ElemType alpha, const Matrix<ElemType>& a);

        Matrix<ElemType> operator* (const Matrix<ElemType>& a) const;
        Matrix<ElemType>& AssignProductOf (const Matrix<ElemType>& a, const bool transposeA, const Matrix<ElemType>& b, const bool transposeB); // this = a * b
        Matrix<ElemType>& Assign1x1ProductOf(const Matrix<ElemType>& a1x1, const Matrix<ElemType>& b); // this = a * b, where a is 1x1

        Matrix<ElemType>& operator/= (ElemType alpha);
        Matrix<ElemType> operator/ (ElemType alpha) const;        

        Matrix<ElemType>& operator^= (ElemType alpha); //element-wise power
        Matrix<ElemType> operator^ (ElemType alpha) const; //element-wise power
        Matrix<ElemType>& AssignElementPowerOf(const Matrix<ElemType>& a, const ElemType power);

        Matrix<ElemType>& ElementMultiplyWith (const Matrix<ElemType>& a);
        Matrix<ElemType>& AssignElementProductOf (const Matrix<ElemType>& a, const Matrix<ElemType>& b);
        Matrix<ElemType>& AddElementProductOf (const Matrix<ElemType>& a, const Matrix<ElemType>& b);

        Matrix<ElemType>& AssignElementDivisionOf (const Matrix<ElemType>& a, const Matrix<ElemType>& b);
        Matrix<ElemType>& ElementDivideBy(const Matrix<ElemType>& a);

        Matrix<ElemType>& ColumnElementMultiplyWith(const Matrix<ElemType>& a);
        Matrix<ElemType>& RowElementMultiplyWith(const Matrix<ElemType>& a);

        Matrix<ElemType>& ColumnElementDivideBy(const Matrix<ElemType>& a);
        Matrix<ElemType>& RowElementDivideBy(const Matrix<ElemType>& a);

        Matrix<ElemType>& ElementInverse ();
        Matrix<ElemType>& AssignElementInverseOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceLinearRectifierDerivative();
        Matrix<ElemType>& AssignLinearRectifierDerivativeOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceSigmoidDerivative();
        Matrix<ElemType>& AssignSigmoidDerivativeOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceSigmoid ();
        Matrix<ElemType>& AssignSigmoidOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceTanh ();
        Matrix<ElemType>& AssignTanhOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceLogSoftmax (const bool isColWise);
        Matrix<ElemType>& AssignLogSoftmaxOf (const Matrix<ElemType>& a, const bool isColWise);

        Matrix<ElemType>& InplaceHardmax(const bool isColWise);
        Matrix<ElemType>& AssignHardmaxOf(const Matrix<ElemType>& a, const bool isColWise);

        //sequence training 
        Matrix<ElemType>& DropFrame(const Matrix<ElemType>& label, const Matrix<ElemType>& gamma, const ElemType & threshhold);
        Matrix<ElemType>& AssignSequenceError(const ElemType hsmoothingWeight, const Matrix<ElemType>& label, const Matrix<ElemType>& dnnoutput, const Matrix<ElemType>& gamma, ElemType alpha);
        Matrix<ElemType>& InplaceSqrt ();
        Matrix<ElemType>& AssignSqrtOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceExp ();
        Matrix<ElemType>& AssignExpOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceLog ();
        Matrix<ElemType>& AssignLogOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceCosine ();
        Matrix<ElemType>& AssignCosineOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceNegativeSine ();
        Matrix<ElemType>& AssignNegativeSineOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceLog10 ();
        Matrix<ElemType>& AssignLog10Of (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceAbs ();
        Matrix<ElemType>& AssignAbsOf (const Matrix<ElemType>& a);

        Matrix<ElemType>& InplaceTruncateBottom (const ElemType threshold);
        Matrix<ElemType>& AssignTruncateBottomOf (const Matrix<ElemType>& a, const ElemType threshold);
        Matrix<ElemType>& InplaceTruncateTop (const ElemType threshold);
        Matrix<ElemType>& AssignTruncateTopOf (const Matrix<ElemType>& a, const ElemType threshold);
        Matrix<ElemType>& InplaceTruncate (const ElemType threshold);
        Matrix<ElemType>& InplaceSoftThreshold(const ElemType threshold);

        Matrix<ElemType>& SetToZeroIfAbsLessThan (const ElemType threshold);

        DeviceBoundNumber<ElemType> Sum_AsDeviceBoundNum() const;
        ElemType SumOfAbsElements () const; //sum of all abs(elements)
        ElemType SumOfElements () const; //sum of all elements
        Matrix<ElemType>& AssignSumOfElements(const Matrix<ElemType>& a);

        ElemType LogAddSumOfElements() const;

        Matrix<ElemType>&  AssignToRowSliceValuesOf(const Matrix<ElemType>& a, const size_t startIndex, const size_t numRows);
        Matrix<ElemType>&  AssignRowSliceValuesOf(const Matrix<ElemType>& a, const size_t startIndex, const size_t numRows);
        Matrix<ElemType>&  AddToRowSliceValuesOf(const Matrix<ElemType>& a, const size_t startIndex, const size_t numRows); 
        Matrix<ElemType>&  AddWithRowSliceValuesOf(const Matrix<ElemType>& a, const size_t startIndex, const size_t numRows);
        //Matrix<ElemType>&  AssignRowStackValuesOf(const std::vector<const Matrix<ElemType>*>& inputMatrices, const size_t sliceStartCol, const size_t sliceNumCols);

        Matrix<ElemType>&  AssignRepeatOf(const Matrix<ElemType>& a, const size_t numRowRepeats, const size_t numColRepeats);
        Matrix<ElemType>&  AddToRowRepeatValuesOf(const Matrix<ElemType>& a, const size_t numRepeats);

        Matrix<ElemType>&  AssignPositiveAndShiftedNegSample(const Matrix<ElemType>& a, const size_t posNumber, const size_t negNumber, const size_t shiftNumber);
        Matrix<ElemType>&  AddFoldedPositiveAndShiftedNegSample(const Matrix<ElemType>& a, const size_t posNumber, const size_t negNumber, const size_t shiftNumber);
        
        bool IsEqualTo(const Matrix<ElemType>& a, const ElemType threshold = 1e-8) const;

        static void VectorSum(const Matrix<ElemType>& a, Matrix<ElemType>& c, const bool isColWise);

        void VectorNorm1(Matrix<ElemType>& c, const bool isColWise) const;
        Matrix<ElemType>& AssignVectorNorm1Of(Matrix<ElemType>& a, const bool isColWise);       // TODO: arg should be const

        void VectorNorm2(Matrix<ElemType>& c, const bool isColWise) const;
        Matrix<ElemType>& AssignVectorNorm2Of(Matrix<ElemType>& a, const bool isColWise);       // TODO: arg should be const

        void VectorNormInf(Matrix<ElemType>& c, const bool isColWise) const;
        Matrix<ElemType>& AssignVectorNormInfOf(Matrix<ElemType>& a, const bool isColWise);

        Matrix<ElemType>& AssignInnerProductOf(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const bool isColWise);
        Matrix<ElemType>& AssignKhatriRaoProductOf(const Matrix<ElemType>& a, const Matrix<ElemType>& b);
        Matrix<ElemType>& AddColumnReshapeProductOf(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const bool transposeAColumn);

        Matrix<ElemType>& AddWithScaleOf(ElemType alpha, const Matrix<ElemType>& a);    // this += alpha * a

        ElemType FrobeniusNorm() const;
        Matrix<ElemType>& AssignFrobeniusNormOf(const Matrix<ElemType>& a);

        ElemType MatrixNormInf() const;
        ElemType MatrixNorm1() const;
        ElemType MatrixNorm0() const; //number of non-zero elemets
        Matrix<ElemType>& AssignSignOf(const Matrix<ElemType>& a);
        Matrix<ElemType>& AddSignOf(const Matrix<ElemType>& a);
        void VectorMax(Matrix<ElemType>& maxIndexes, Matrix<ElemType>& maxValues, const bool isColWise) const;
        void VectorMax(Matrix<ElemType>& maxIndexes, Matrix<ElemType>& maxValues, const bool isColWise, int topK) const;
        void VectorMin(Matrix<ElemType>& minIndexes, Matrix<ElemType>& minValues, const bool isColWise) const;

        Matrix<ElemType>&  AssignNumOfDiff(const Matrix<ElemType>& a, const Matrix<ElemType>& b, bool searchInCol = false); 

        Matrix<ElemType>& AssignInnerProductOfMatrices(const Matrix<ElemType>& a, const Matrix<ElemType>& b); //this method will resize(1,1) first

        bool HasNan (const char * name) const;
        size_t CountNanInf() const;

        void Print(const char* matrixName, size_t rowStart, size_t rowEnd, size_t colStart, size_t colEnd) const;
        void Print(const char* matrixName = nullptr) const; //print whole matrix. can be expensive

        Matrix<ElemType>& AssignPackedConvolutionInput(const Matrix<ElemType>& inputSubBatch, 
                                                 const size_t inputWidth, const size_t inputHeight, const size_t inputChannels,
                                                 const size_t outputWidth, const size_t outputHeight, const size_t outputChannels,
                                                 const size_t kernelWidth, const size_t kernelHeight, const size_t horizontalSubsample, const size_t verticalSubsample, 
                                                 const bool zeroPadding = false); 
        Matrix<ElemType>& UnpackConvolutionInput(Matrix<ElemType>& inputSubBatch, 
                                                 const size_t inputWidth, const size_t inputHeight, const size_t inputChannels,
                                                 const size_t outputWidth, const size_t outputHeight, const size_t outputChannels,
                                                 const size_t kernelWidth, const size_t kernelHeight, const size_t horizontalSubsample, const size_t verticalSubsample, 
                                                 const bool zeroPadding = false) const; 
        Matrix<ElemType>& AssignMaxPoolingResult(const Matrix<ElemType>& inputBatch, const size_t channels, 
                                                 const size_t inputWidth, const size_t inputHeight,  const size_t inputSizePerSample, 
                                                 const size_t outputWidth, const size_t outputHeight, const size_t outputSizePerSample, 
                                                 const size_t windowWidth, const size_t windowHeight, const size_t horizontalSubsample, const size_t verticalSubsample);
        Matrix<ElemType>& AddMaxPoolingGradient(const Matrix<ElemType>& outputGradientBatch, const Matrix<ElemType>& inputBatch, const Matrix<ElemType>& outputBatch, 
                                                 const size_t channels, 
                                                 const size_t inputWidth, const size_t inputHeight, const size_t inputSizePerSample, 
                                                 const size_t outputWidth, const size_t outputHeight, const size_t outputSizePerSample, 
                                                 const size_t windowWidth, const size_t windowHeight, const size_t horizontalSubsample, const size_t verticalSubsample);
        Matrix<ElemType>& AssignAveragePoolingResult(const Matrix<ElemType>& inputBatch, const size_t channels, 
                                                 const size_t inputWidth, const size_t inputHeight,  const size_t inputSizePerSample, 
                                                 const size_t outputWidth, const size_t outputHeight, const size_t outputSizePerSample, 
                                                 const size_t windowWidth, const size_t windowHeight, const size_t horizontalSubsample, const size_t verticalSubsample);
        Matrix<ElemType>& AddAveragePoolingGradient(const Matrix<ElemType>& outputGradientBatch, 
                                                 const size_t channels, 
                                                 const size_t inputWidth, const size_t inputHeight, const size_t inputSizePerSample, 
                                                 const size_t outputWidth, const size_t outputHeight, const size_t outputSizePerSample, 
                                                 const size_t windowWidth, const size_t windowHeight, const size_t horizontalSubsample, const size_t verticalSubsample);
    public:
        // TODO: why are these not static? And why are they here?
        ElemType Exp10(ElemType num); 
        ElemType Mod(ElemType x , ElemType y);
        ElemType LogAdd(ElemType x, ElemType y);

    public:
        static DEVICEID_TYPE GetBestGPUDeviceId();

        //static BLAS functions

        // singular value decomposition of A as A = U*SIGMA*VT
        static void SVD(const Matrix<ElemType>& A, Matrix<ElemType>& SIGMA, Matrix<ElemType>& U, Matrix<ElemType>& VT, Matrix<ElemType>& W);

        static void MultiplyAndWeightedAdd(ElemType alpha, const Matrix<ElemType>& a, const bool transposeA, const Matrix<ElemType>& b, const bool transposeB, ElemType beta, Matrix<ElemType>& c); // SGEMM
        static void MultiplyAndAdd(const Matrix<ElemType>& a, const bool transposeA, const Matrix<ElemType>& b, const bool transposeB, Matrix<ElemType>& c);
        static void Multiply(const Matrix<ElemType>& a, const bool transposeA, const Matrix<ElemType>& b, const bool transposeB, Matrix<ElemType>& c);
        static void Multiply(const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c);
        static void Multiply1x1AndWeightedAdd(ElemType alpha, const Matrix<ElemType>& a, const Matrix<ElemType>& b, ElemType beta, Matrix<ElemType>& c);
        static void ConvolveAndWeightedAdd(ElemType alpha, const Matrix<ElemType>& a, const Matrix<ElemType>& b, ElemType beta, Matrix<ElemType>& c, size_t imageWidth, size_t kernelWidth, size_t stepSize, bool padding);

        static void ScaleAndAdd(ElemType alpha, const Matrix<ElemType>& a, Matrix<ElemType>& c);
        static void ScaleAndAdd(ElemType alpha, const Matrix<ElemType>& a, ElemType beta, Matrix<ElemType>& c);
        static void AddScaledDifference(const ElemType alpha, const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c);
        static void AssignScaledDifference(const ElemType alpha, const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c);
        static void AddScaledDifference(const Matrix<ElemType>& alpha, const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c);  // c += alpha * (a - b)
        static void AssignScaledDifference(const Matrix<ElemType>& alpha, const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c);

        static void AddElementToElement(const Matrix<ElemType>& a, const size_t ai, const size_t aj, Matrix<ElemType>& c, const size_t ci, const size_t cj); 
        //static void AddLogElementToElement(const Matrix<ElemType>& a, const size_t ai, const size_t aj, Matrix<ElemType>& c, const size_t ci, const size_t cj); 
        static void AssignElementToElement(const Matrix<ElemType>& a, const size_t ai, const size_t aj, Matrix<ElemType>& c, const size_t ci, const size_t cj); 
        static void MinusOneAt(Matrix<ElemType>& c, const size_t position);

        static void Scale(ElemType alpha, Matrix<ElemType>& a);
        static void Scale(const Matrix<ElemType>& alpha, Matrix<ElemType>& a); //In this case Matrix alpha must be 1x1
        static void Scale(ElemType alpha, const Matrix<ElemType>& a, Matrix<ElemType>& c);
        static void InnerProduct (const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c, const bool isColWise);
        static ElemType InnerProductOfMatrices(const Matrix<ElemType>& a, const Matrix<ElemType>& b);
        static void ElementWisePower (ElemType alpha, const Matrix<ElemType>& a, Matrix<ElemType>& c);

        static bool AreEqual(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const ElemType threshold = 1e-8);
        static bool HasElement(const Matrix<ElemType>& a, const ElemType value = 0.0);

        static void TensorShuffleScaleAndAdd(ElemType keepWeight, const Matrix<ElemType>& a, size_t D, size_t S, size_t M, size_t K, size_t T, ElemType scaleFactor, const Matrix<ElemType>& b, Matrix<ElemType>& c);
    public:
        void Read(File& stream);
        void Write(File& stream) const;

        Matrix<ElemType>& Shift(const Matrix<ElemType>& a, int shift);

        Matrix<ElemType>& AssignElementProductOfWithShiftNeg(const Matrix<ElemType>& a, const Matrix<ElemType>& b, size_t shift, size_t negnumber);
        Matrix<ElemType>& AssignInnerProductOfWithShiftNeg(const Matrix<ElemType>& a, const Matrix<ElemType>& b, const bool isColWise, size_t shift, size_t negnumber);
        static void InnerProductWithShiftNeg(const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c, const bool isColWise, size_t shift, size_t negnumber);
        Matrix<ElemType>& GetARowByIndex(const Matrix<ElemType>& a, size_t index);
        static void ConductRowElementMultiplyWithShift(const Matrix<ElemType>& a, const Matrix<ElemType>& b, Matrix<ElemType>& c, size_t shift, bool bFirstmatrixfixed);
        Matrix<ElemType>& AssignElementProductOfWithShift(const Matrix<ElemType>& a, const Matrix<ElemType>& b, size_t shift);

    public:
        static void RCRFBackwardCompute(const Matrix<ElemType>& alpha, Matrix<ElemType>& beta,
            Matrix<ElemType>& functionValues, const Matrix<ElemType>& lbls,
            const Matrix<ElemType>& pos_scores, const Matrix<ElemType>& pair_scores, const int shift);

        static void RCRFTransGrdCompute(const Matrix<ElemType>& lbls,
            const Matrix<ElemType>&   alpha,
            const Matrix<ElemType>& beta,
            const Matrix<ElemType>& pair_scores,
            Matrix<ElemType>& grd,
            const int startLbl, /// the time 0 start symbol in the output layer
            const int shift);

        template<typename T>
        friend class MatrixQuantizer;

        template<typename T>
        friend class QuantizedMatrix;

        template<typename T>
        friend class Matrix;
    };

    // overload I/O operators
    template<class ElemType>
    File& operator>>(File& stream, Matrix<ElemType>& M) { M.Read(stream); return stream; }
    template<class ElemType>
    File& operator<<(File& stream, const Matrix<ElemType>& M) { M.Write(stream); return stream; }

    typedef Matrix<float> SingleMatrix;
    typedef Matrix<double> DoubleMatrix;
}}}
