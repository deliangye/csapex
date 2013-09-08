#ifndef TERRAMAT
#define TERRAMAT

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct TerrainClass {
    TerrainClass(uchar id, std::string name, cv::Vec3b color)
        :
          id(id),
          name(name),
          color(color) {
    }

    TerrainClass()
    {
    }

    void write(cv::FileStorage &store) const
    {
        store << "{:";
        store << "id" << id;
        store << "name" << name;
        store << "color" << "[:" << color[0] << color[1] << color[2] << "]";
        store << "}";
    }

    void read(const cv::FileNode &store)
    {
        store["id"] >> id;
        store["name"] >> name;

        std::vector<uchar> value;
        store["color"] >> value;
        color = cv::Vec3b(value[0], value[1], value[2]);
    }

    uchar       id;
    std::string name;
    cv::Vec3b   color;
};


class TerraMat
{
public:
    TerraMat();
    TerraMat(const cv::Mat &terra_mat);
    TerraMat(const cv::Mat &terra_mat, const std::map<int, int> &mapping);


    void setMatrix(const cv::Mat &terra_mat);
    void setMatrix(const cv::Mat &terra_mat, const std::map<int, int> &mapping);
    cv::Mat getMatrix() const;
    void setMapping(const std::map<int, int> &mapping);
    std::map<int, int> getMapping() const;
    void addLegendEntry(TerrainClass terrainClass);
    std::map<int, TerrainClass> getLegend() const;

    /// I / O - Persistence
    void read(const std::string &filename);
    void write(const std::string &filename) const;

    // exports an uchar image, each pixel containing the id of the favorite terrain class
    cv::Mat getFavorites();
    cv::Mat getFavorites(cv::Mat &validity, const float thresh = 0.5f);

    // get channel related to a terrain id
    int     getChannelOfId(const int id);

    // exports an rgb image showing the color of the favorite terrain class in each pixel
    cv::Mat getFavoritesBGR();
    // exports the maximum aknowledge classes
    cv::Mat getFavoritesBGRRaw();

    // matrix where probabilities are absolut, there for one channel holding prob = 1.f
    void getAbsolut(TerraMat &abs);

    // set a channel as absolut
    void setAbsolut(const int row, const int col, const int channel);

    // set all channels zero, so that terrain class is unknown
    void setUnknown(const int row, const int col);

    // exports an rgb image showing the weighted mean color of the terrain class in each pixel
    cv::Mat getMeanBGR();

    // exports a hybrid rgb image of getMeanBGR and getFavoritesBGR
    cv::Mat getBGR();

    operator cv::Mat();
    operator cv::Mat&();

    template<typename _Tp>
    _Tp& at(int i, int j)
    {
        return terra_mat_.at<_Tp>(i,j);
    }

private:
    int                         channels_;
    int                         step_;
    std::map<int,int>           mapping_;
    std::map<int,int>           reverse_mapping_;
    std::map<int,TerrainClass>  legend_;
    cv::Mat                     terra_mat_;

    cv::Vec3b                   COLOR_INVALID;

    void writeMapping(cv::FileStorage &fs) const;
    void readMapping(const cv::FileStorage &fs);
    void writeLegend(cv::FileStorage &fs) const;
    void readLegend(const cv::FileStorage &fs);
    void writeMatrix(cv::FileStorage &fs) const;
    void readMatrix(const cv::FileStorage &fs);
    void calcReversMapping();
};


#endif
