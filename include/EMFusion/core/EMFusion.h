/*
 * This file is part of EM-Fusion.
 *
 * Copyright (C) 2020 Embodied Vision Group, Max Planck Institute for Intelligent Systems, Germany.
 * Developed by Michael Strecke <mstrecke at tue dot mpg dot de>.
 * For more information see <https://emfusion.is.tue.mpg.de>.
 * If you use this code, please cite the respective publication as
 * listed on the website.
 *
 * EM-Fusion is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EM-Fusion is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EM-Fusion.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <numeric>

#include <boost/filesystem.hpp>

#include <opencv2/opencv.hpp>
#include <opencv2/viz.hpp>
#include <opencv2/cudalegacy.hpp>

#include "EMFusion/core/data.h"
#include "EMFusion/core/TSDF.h"
#include "EMFusion/core/ObjTSDF.h"

#include "EMFusion/utils/data.h"

#include "EMFusion/core/MaskRCNN.h"

namespace emf {

/**
 * Class for processing frames in the EM-Fusion framework and maintaining
 * models.
 */
class EMFusion {
public:
    /**
     * Constructor for EMFusion class.
     *
     * @param _params Camera and algorithm paramerters
     */
    EMFusion ( const Params& _params );

    /**
     * Reset algorithm state to initial.
     */
    void reset();

    /**
     * Process a new frame (tracking and mapping of background and objects).
     *
     * @param frame input frame
     */
    void processFrame ( const RGBD& frame );

    /**
     * Render the current model state from the current camera pose via
     * raycasting. Optionally also display meshes generated by marching cubes
     * and bounding boxes of the objects.
     *
     * @param rendered the image to store the rendered output.
     * @param window pointer to an optional Viz3d window for mesh rendering.
     */
    void render ( cv::Mat& rendered, cv::viz::Viz3d* window = NULL );

    /**
     * Get the last Mask R-CNN segmentation image.
     *
     * @param maskim the image to store the segmentation.
     */
    void getLastMasks ( cv::Mat& maskim );

    /**
     * Prepare for saving results to files.
     *
     * @param exp_frame_meshes whether to output meshes for every frame
     * @param exp_vols whether to output volume data.
     */
    void setupOutput ( bool exp_frame_meshes, bool exp_vols );

    /**
     * Prepare for using preprocessed masks.
     *
     * @param path the path in which the mask files are saved.
     */
    void usePreprocMasks ( const std::string& path );

    /**
     * Write results to a folder.
     *
     * @param path the path where results are stored (will be created and should
     *             not exist).
     */
    void writeResults ( const std::string& path );

protected:
    /**
     * Preprocess input depth maps with bilateral filter.
     *
     * @param depth_raw raw input depthmap.
     * @param depth processed depthmap.
     */
    void preprocessDepth ( const cv::cuda::GpuMat& depth_raw,
                           cv::cuda::GpuMat& depth );

    /**
     * Store association likelihoods for writing to files after end of program.
     *
     * @param bg_associationWeights current background assoc likelihoods
     * @param bg_assocWeights_frame per-frame map to add the background
     *                              association likelihood to
     * @param assocs map storing current association weights for objects
     * @param obj_assocs_frame per-frame map to add object association
     *                               likelihoods to
     */
    void storeAssocs ( const cv::cuda::GpuMat& bg_associationWeights,
                       std::map<int, cv::Mat>& bg_assocWeights_frame,
                       const std::map<int, cv::cuda::GpuMat>& assocs,
                       std::map<int, std::map<int, cv::Mat>>& assocs_frame );

    /**
     * Store current poses for later writing to output.
     */
    void storePoses ();

    /**
     * Check for new objects and initialize them.
     *
     * @param rgb input rgb image
     * @param matches the matches of object ids and Mask R-CNN segmentations
     *
     * @return number of detected instances by Mask R-CNN.
     */
    int initOrMatchObjs ( const cv::Mat& rgb,
                          std::map<int, cv::cuda::GpuMat>& matches );

    /**
     * Run Mask R-CNN (or load preprocessed segmentations.
     *
     * @param rgb input rgb image
     * @param bounding_boxes output bounding boxes
     * @param segmentation output segmentations
     * @param scores output class scores
     */
    int runMaskRCNN ( const cv::Mat& rgb,
                      std::vector<cv::Rect>& bounding_boxes,
                      std::vector<cv::Mat>& segmentation,
                      std::vector<std::vector<double>>& scores );

    /**
     * Compute valid points from measurement pointcloud.
     *
     * @param points input pointcloud
     * @param validPoints output mask
     */
    void computeValidPoints ( const cv::cuda::GpuMat& points,
                              cv::cuda::GpuMat& validPoints );

    /**
     * Transform points with a given transformation.
     *
     * @param points input points
     * @param pose input transformation
     * @param points_w output transformed points
     */
    void transformPoints ( const cv::cuda::GpuMat& points,
                           const cv::Affine3f& pose,
                           cv::cuda::GpuMat& points_w );

    /**
     * Match Mask R-CNN detections to existing objects.
     *
     * @param seg_gpus Mask R-CNN masks
     * @param matches map from object IDs to matched masks
     * @param score_matches map from object IDs to class scores for the match
     * @param unmatchedMasks set of unmatched masks IDs for initialization of
     *                       new objects
     */
    void matchSegmentation ( const std::vector<cv::cuda::GpuMat>& seg_gpus,
                             const std::vector<std::vector<double>>& scores,
                             std::map<int, cv::cuda::GpuMat>& matches,
                             std::map<int, std::vector<double>>& score_matches,
                             std::set<int>& unmatchedMasks );

    /**
     * Initialize new object volumes using unmatched masks.
     *
     * @param seg_gpus the Mask R-CNN detections
     * @param scores the class scores for the detections
     * @param unmatchedMasks the unmatched mask ids
     * @param matches the matched detections (will contain matches for new
     *                objects)
     * @param score_matches the scores for the matched detections  (will contain
     *                      matches for new objects)
     */
    void initObjsFromUnmatched (
        std::vector<cv::cuda::GpuMat>& seg_gpus,
        const std::vector<std::vector<double>>& scores,
        const std::set<int>& unmatchedMasks,
        std::map<int, cv::cuda::GpuMat>& matches,
        std::map<int, std::vector<double>>& score_matches );

    /**
     * Initialize new object volume.
     *
     * @param mask the object mask from Mask R-CNN
     * @param points the pointcloud in world coordinates
     * @param pose the current camera pose
     *
     * @return class ID of new object, -1 if not initialized
     */
    int initNewObjVolume ( const cv::cuda::GpuMat& mask,
                           const cv::cuda::GpuMat& points,
                           const cv::Affine3f& pose );

    /**
     * Compute volumetric IOU with existing object for given percentiles
     *
     * @param obj object to check
     * @param p10 10th percentile from new object pointcloud
     * @param p90 90th percentile from new object pointcloud
     *
     * @return volumetric IOU of potential new volume and obj
     */
    float volumeIOU ( const ObjTSDF& obj, const cv::Vec3f& p10,
                      const cv::Vec3f& p90 );

    /**
     * Generate an array of random colors for visualization.
     *
     * @return random colormap.
     */
    cv::Mat randomColors ();

    /**
     * Compute association weights for background and objects.
     */
    void computeAssociationWeights();

    /**
     * Run tracking algorithm (background for camera pose update and then
     * objects).
     */
    void performTracking();

    /**
     * Raycast the current model.
     */
    void raycast ();

    /**
     * Match Mask R-CNN segmentation to existing model.
     *
     * @param new_seg segmentation from Mask R-CNN.
     * @param match_iou the IOU of the best match, to be checked for better fit
     *                  with other masks.
     *
     * @return object ID of match, -1 if no match
     */
    int matchSegmentation ( const cv::cuda::GpuMat& new_seg, float& match_iou );

    /**
     * Update object (resize if necessary and update class scores).
     *
     * @param obj the object to be updated
     * @param points pointcloud from current frame
     * @param seg_gpu new Mask R-CNN mask matched with obj
     * @param scores class score distribution for new mask
     *
     * @return the pose offset for the object generated by the update
     */
    cv::Vec3f updateObj ( ObjTSDF& obj, const cv::cuda::GpuMat& points,
                          const cv::cuda::GpuMat& seg_gpu,
                          const std::vector<double>& scores );

    /**
     * Integrate depth measurements in background and object models.
     */
    void integrateDepth ();

    /**
     * Integrate the new masks as foreground probabilities.
     *
     * @param matches the matches of Mask R-CNN segmentations to object IDs.
     */
    void integrateMasks ( const std::map<int, cv::cuda::GpuMat>& matches );

    /**
     * Initialize caching and output variables for new object.
     *
     * @param id the id for the new object
     */
    void createObj ( const int id );

    /**
     * Clean up objects that cannot be tracked reliable anymore.
     *
     * @param numInstances number of Mask R-CNN segmentations
     * @param matches matches of object IDs to segmentations
     */
    void cleanUpObjs ( int numInstances,
                       const std::map<int, cv::cuda::GpuMat>& matches );

    /**
     * Delete object from caching variables.
     *
     * @param id the id of the deleted object
     */
    void deleteObj ( const int id );

    /**
     * Write poses to files.
     *
     * @param p path to write the results to.
     */
    void writePoses ( const boost::filesystem::path& p );

    /**
     * Write renderings to files.
     *
     * @param p path to write the results to.
     */
    void writeRenderings ( const boost::filesystem::path& p );

    /**
     * Write mesh visualizations to files.
     *
     * @param p path to write the results to.
     */
    void writeMeshVis ( const boost::filesystem::path& p );

    /**
     * Write mask visualizations to files.
     *
     * @param p path to write the results to.
     */
    void writeMasks ( const boost::filesystem::path& p );

    /**
     * Write association weights to files.
     *
     * @param p path to write the results to.
     */
    void writeAssocs ( const boost::filesystem::path& p );

    /**
     * Write huber weights to files.
     *
     * @param p path to write the results to.
     */
    void writeHuberWeights ( const boost::filesystem::path& p );

    /**
     * Write tracking weights to files.
     *
     * @param p path to write the results to.
     */
    void writeTrackWeights ( const boost::filesystem::path& p );

    /**
     * Write foreground probabilities to files.
     *
     * @param p path to write the results to.
     */
    void writeFgProbs ( const boost::filesystem::path& p );

    /**
     * Write meshes to files.
     *
     * @param p path to write the results to.
     */
    void writeMeshes ( const boost::filesystem::path& p );

    /**
     * Write TSDF volumes to files.
     *
     * @param p path to write the results to.
     */
    void writeTSDFs ( const boost::filesystem::path& p );

    /**
     * Add offsets created by object resizing to the object poses for
     * evaluation.
     *
     * @param poses the original object poses
     * @param offsets the offsets created by resizing
     * @return the corrected poses
     */
    std::map<int, std::map<int, cv::Affine3f>> addPoseOffsets (
            const std::map<int, std::map<int, cv::Affine3f>>& poses,
            const std::map<int, std::map<int, cv::Vec3f>>& offsets );

    /**
     * Write array of poses to txt file.
     *
     * @param filename the name of the file to write to.
     * @param poses map of frames to poses
     */
    void writePoseFile ( const std::string& filename,
                         const std::map<int, cv::Affine3f>& poses );

    /**
     * Write a numbered image to file.
     *
     * @param path the path to write the image to
     * @param id   the image id (numer for filename)
     * @param image the image to write
     */
    void writeImage ( const boost::filesystem::path& path, int id,
                      const cv::Mat& image );

    /**
     * Write mesh of an Object to file.
     *
     * @param mesh the mesh to be written
     * @param filename the name of the output file
     */
    void writeMesh ( const cv::viz::Mesh& mesh, const std::string& filename );

    /**
     * Write volume to binary file.
     * File structure: First 3 numbers are 32-bit integers giving the
     * resolution. Then, a size_t gives the size of a single volume element.
     * Afterwards, a 32-bit float gives the metric size of a single voxel.
     * Finally, the rest of the file contains the volume elements.
     *
     * @param filename the file name to write the data to.
     * @param vol the volume to write
     * @param resolution the resolution of the written volume
     * @param voxelSize the voxel size.
     */
    void writeVolume ( const std::string& filename, const cv::Mat& vol,
                       const cv::Vec3i& resolution, float voxelSize );

    /** Algorithm parameters. */
    Params params;
    /** Current camera pose. */
    cv::Affine3f pose;
    /** List of currently maintained objects. */
    std::list<ObjTSDF> objects;
    /** Coarse background volume for camera tracking. */
    TSDF background;

    /** Object for loading and executing Mask R-CNN. */
    MaskRCNN maskrcnn;
    std::string maskPath;

    /** Color map for visualizing objects. */
    cv::Mat colorMap;
    /** Frame counter. */
    int frameCount;
    /** Indicator for saving outputs to file after processing. */
    bool saveOutput = false;
    /** Indicator for saving meshes for every frame. */
    bool expFrameMeshes = false;
    /** Indicator for saving volume data for output. */
    bool expVols = false;
    /** Streams for objects to parallelize processing. */
    std::map<int, cv::cuda::Stream> streams;

    std::set<int> vis_objs;

    // Cache some arrays to speed up processing
    cv::cuda::GpuMat points, points_w, depth, depth_raw, depth_mask;
    cv::cuda::GpuMat raylengths, vertices, normals, modelSegmentation;
    std::map<int, cv::cuda::GpuMat> obj_raylengths, obj_vertices, obj_normals,
        obj_modelSegmentation;
    cv::cuda::GpuMat bg_raylengths, bg_vertices, bg_normals, bg_mask;
    cv::cuda::GpuMat bg_modelSegmentation;
    cv::cuda::GpuMat diffRaylengths, takeBgMask, noObjMask;
    cv::cuda::GpuMat mask, image;
    cv::cuda::GpuMat obj_mask, seg_inter, seg_uni;
    cv::cuda::GpuMat validPoints;
    std::vector<cv::cuda::GpuMat> seg_gpus;
    cv::cuda::GpuMat associationNorm;
    cv::cuda::GpuMat bg_associationWeights;
    std::map<int, cv::cuda::GpuMat> associationWeights;

    // Results to be written to output files
    std::map<int, cv::Affine3f> poses;
    std::map<int, std::map<int, cv::Affine3f>> obj_poses;
    std::map<int, std::map<int, cv::Vec3f>> obj_pose_offsets;
    std::map<int, cv::Mat> renderings, mesh_vis, mask_vis,
        bg_assocWeight_preTrack, bg_assocWeight_postTrack,
        bg_trackWeights, bg_huberWeights;
    std::map<int, std::map<int, cv::Mat>> obj_assocWeights_preTrack,
        obj_assocWeights_postTrack, obj_trackWeights, obj_huberWeights,
        obj_fgProbs;

    std::map<int, cv::Mat> tsdfs;
    std::map<int, cv::Mat> intWeights;
    std::map<int, cv::Mat> fgProbs;
    std::map<int, std::pair<cv::Vec3i, float>> meta;

    std::map<int, cv::viz::Mesh> meshes;
    std::map<int, cv::viz::Mesh> frame_meshes;
    std::map<int, std::map<int, cv::viz::Mesh>> frame_obj_meshes;
};

}
