
#ifndef __MAT__
#define __MAT__

using namespace std;

#include <vector>
#include <string>
#include <fstream>

class Mat
{
    private:
        vector<vector<double> > mat;

        //void newMat(int rows, int cols, double defaultValue);

    public:
        Mat& operator=(const Mat&);

        Mat(int rows, int cols, double defaultValue);
        Mat(int rows, int cols);
        void set(int row, int col, double value);
        double get(int row, int col) const;
        int rows() const;
        int cols() const;
        int size() const;
        void scalar(double scalar);
        void transpose();
        Mat mult(const Mat &b) const;
        Mat directMult(const Mat &b) const;
        Mat sum(const Mat &b) const;
        Mat sub(const Mat &b) const;
        Mat copy() const;
        void print(string = " ") const;
        void print(ofstream&, string = " ") const;

};

#endif