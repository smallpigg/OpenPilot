/**
 * \file rawProcessors.hpp
 *
 *  Some wrappers to raw processors to be used generically
 *
 * \date 20/07/2010
 * \author croussil@laas.fr
 *
 * ## Add detailed description here ##
 *
 * \ingroup rtslam
 */

#ifndef RAWSEGPROCESSORS_HPP
#define RAWSEGPROCESSORS_HPP

#ifdef HAVE_MODULE_DSEG

#include "dseg/DirectSegmentsTracker.hpp"
#include "dseg/SegmentHypothesis.hpp"
#include "dseg/SegmentsSet.hpp"
#include "dseg/ConstantVelocityPredictor.hpp"
#include "dseg/RtslamPredictor.hpp"
#include "dseg/HierarchicalDirectSegmentsDetector.hpp"
#include "dseg/GradientStatsDescriptor.hpp"

#include "rtslam/rawImage.hpp"
#include "rtslam/sensorPinhole.hpp"
#include "rtslam/descriptorImageSeg.hpp"

namespace jafar {
namespace rtslam {

	boost::weak_ptr<RawImage> lastDsegImage;
	dseg::PreprocessedImage preprocDsegImage;

   class DsegMatcher
   {
      private:
         dseg::DirectSegmentsTracker matcher;
         dseg::RtslamPredictor predictor;

      public:
         struct matcher_params_t {
            // RANSAC
            int maxSearchSize;
            double lowInnov;      ///<     search region radius for first RANSAC consensus
            double threshold;     ///<     matching threshold
            double mahalanobisTh; ///< Mahalanobis distance for outlier rejection
            double relevanceTh; ///< Mahalanobis distance for no information rejection
            double measStd;       ///<       measurement noise std deviation
            double measVar;       ///<       measurement noise variance
         } params;

      private :
			void projectExtremities(const vec4& meas, const vec4& exp, vec4& newMeas, float* stdRatio) const
			{
            // extract predicted points
            vec2 P1 = subrange(exp,0,2);
				vec2 P2 = subrange(exp,2,4);
				double P12_2 = (P2(0) - P1(0))*(P2(0) - P1(0)) // Square(distance(P1,P2))
							  +   (P2(1) - P1(1))*(P2(1) - P1(1));
				double P12 = sqrt(P12_2);
            // extract measured line
            vec2 L1 = subrange(meas,0,2);
            vec2 L2 = subrange(meas,2,4);
				double L12_2 = (L2(0) - L1(0))*(L2(0) - L1(0)) // Square(distance(L1,L2))
							+   (L2(1) - L1(1))*(L2(1) - L1(1));
				double L12 = sqrt(L12_2);

				// compute predicted center
				vec2 Pc = (P1 + P2) / 2;
				// project on measured line
				double u = (((Pc(0) - L1(0))*(L2(0) - L1(0)))
							  +((Pc(1) - L1(1))*(L2(1) - L1(1))))
							  /(L12_2);
				vec2 Lc = L1 + u*(L2 - L1);

				// compute measured orientation
				double angle = atan2(L2(1) - L1(1), L2(0) - L1(0));

				// compute extremities
				newMeas[0] = Lc[0] - P12 * cos(angle) / 2;
				newMeas[1] = Lc[1] - P12 * sin(angle) / 2;
				newMeas[2] = Lc[0] + P12 * cos(angle) / 2;
				newMeas[3] = Lc[1] + P12 * sin(angle) / 2;
/*
            // TODO : be carefull L1 != L2
            // project predicted points on line
            double u = (((P1(0) - L1(0))+(L2(0) - L1(0)))
                       +((P1(1) - L1(1))+(L2(1) - L1(1))))
                       /(norm_1(L1 - L2) * norm_1(L1 - L2));
            subrange(newMeas,0,2) = L1 + u*(L2 - L1);

				u = (((P2(0) - L2(0))+(L1(0) - L2(0)))
					 +((P2(1) - L2(1))+(L1(1) - L2(1))))
                /(norm_1(L1 - L2) * norm_1(L1 - L2));
				subrange(newMeas,2,4) = L2 + u*(L1 - L2);
*/

				*stdRatio = P12 / L12;
         }

      public:
         DsegMatcher(double lowInnov, double threshold, double mahalanobisTh, double relevanceTh, double measStd):
            matcher(), predictor()
         {
            params.lowInnov = lowInnov;
            params.threshold = threshold;
            params.mahalanobisTh = mahalanobisTh;
            params.relevanceTh = relevanceTh;
            params.measStd = measStd;
         }

         void match(const boost::shared_ptr<RawImage> & rawPtr, const appearance_ptr_t & targetApp, const image::ConvexRoi & roi, Measurement & measure, appearance_ptr_t & app)
			{
				if(rawPtr != lastDsegImage.lock())
				{
					matcher.preprocessImage(*(rawPtr->img),preprocDsegImage);
					lastDsegImage = rawPtr;
				}

				app_img_seg_ptr_t targetAppSpec = SPTR_CAST<AppearanceImageSegment>(targetApp);
				app_img_seg_ptr_t appSpec = SPTR_CAST<AppearanceImageSegment>(app);

            dseg::SegmentsSet setin, setout;

            setin.addSegment(targetAppSpec->hypothesis());
				matcher.trackSegment(preprocDsegImage,setin,&predictor,setout);

            if(setout.count() > 0) {
					vec4 pred;
					vec4 obs;
					vec4 projected;
					float ratio;

					pred(0) = targetAppSpec->hypothesis()->x1();
					pred(1) = targetAppSpec->hypothesis()->y1();
					pred(2) = targetAppSpec->hypothesis()->x2();
					pred(3) = targetAppSpec->hypothesis()->y2();

					obs(0) = setout.segmentAt(0)->x1();
					obs(1) = setout.segmentAt(0)->y1();
					obs(2) = setout.segmentAt(0)->x2();
					obs(3) = setout.segmentAt(0)->y2();

					projectExtremities(obs,pred, projected, &ratio);
					measure.x() = projected;
					measure.std(params.measStd * ratio);
					measure.matchScore = 1;

               appSpec->setHypothesis(setout.segmentAt(0));
				}
            else {
               measure.matchScore = 0;
            }
         }
   };

   class HDsegDetector
   {
      private:
			dseg::HierarchicalDirectSegmentsDetector detector;
         boost::shared_ptr<DescriptorFactoryAbstract> descFactory;

      public:
         struct detector_params_t {
				int patchSize;  ///<       descriptor patch size
				// RANSAC
            double measStd;       ///<       measurement noise std deviation
            double measVar;       ///<       measurement noise variance
            // HDSEG
            int hierarchyLevel;
         } params;

      public:
			HDsegDetector(int patchSize, int hierarchyLevel, double measStd,
            boost::shared_ptr<DescriptorFactoryAbstract> const &descFactory):
            detector(), descFactory(descFactory)
         {
				params.patchSize = patchSize;
				params.hierarchyLevel = hierarchyLevel;
            params.measStd = measStd;
            params.measVar = measStd * measStd;
         }

			bool detect(const boost::shared_ptr<RawImage> & rawData, const image::ConvexRoi &roi, boost::shared_ptr<FeatureImageSegment> & featPtr)
         {
            bool ret = false;
				featPtr.reset(new FeatureImageSegment());
            featPtr->measurement.std(params.measStd);

				dseg::SegmentsSet set;
				detector.detectSegment(*(rawData->img.get()), &roi, set);

				if(set.count() > 0)
				{
					int bestId = -1;

					double bestSqrLength = -1;
					for(size_t i=0 ; i<set.count() ; i++)
					{
						const dseg::SegmentHypothesis* seg = set.segmentAt(i);

						double dx = seg->x1() - seg->x2();
						double dy = seg->y1() - seg->y2();
						double sqrLength = sqrt(dx*dx + dy*dy);
						sqrLength *= seg->gradientDescriptor().meanGradients();

						// If this segment is longer than the previous best
						if(sqrLength > bestSqrLength)
						{
								vec2 v1,v2;

								v1[0] = seg->x1();
								v1[1] = seg->y1();
								v2[0] = seg->x2();
								v2[1] = seg->y2();

								if(roi.isIn(v1) || roi.isIn(v2))
								{
									// Consider this segment as
									bestId = i;
									bestSqrLength = sqrLength;
								}
						}
					}

					if(bestId >= 0)
					{
						featPtr->measurement.x(0) = set.segmentAt(bestId)->x1();
						featPtr->measurement.x(1) = set.segmentAt(bestId)->y1();
						featPtr->measurement.x(2) = set.segmentAt(bestId)->x2();
						featPtr->measurement.x(3) = set.segmentAt(bestId)->y2();
						featPtr->measurement.matchScore = 1;

						featPtr->appearancePtr.reset(new AppearanceImageSegment(params.patchSize, params.patchSize, JfrImage_CS_GRAY, set.segmentAt(bestId)));

						// extract appearance
						vec pix = featPtr->measurement.x();
						vec2 center;
						center[0] = ( pix[0] + pix[2] )/2;
						center[1] = ( pix[1] + pix[3] )/2;

						boost::shared_ptr<AppearanceImageSegment> appPtr = SPTR_CAST<AppearanceImageSegment>(featPtr->appearancePtr);
						rawData->img->extractPatch(appPtr->patch, (int)center(0), (int)center(1), params.patchSize, params.patchSize);
						appPtr->offsetTop.x()(0) = pix(0) - ((int)center(0) - params.patchSize);
						appPtr->offsetTop.x()(1) = pix(1) - ((int)center(1) - params.patchSize);
						appPtr->offsetTop.P() = jblas::zero_mat(2); // by definition this is our landmark projection

						appPtr->offsetBottom.x()(0) = pix(2) - ((int)center(0) - params.patchSize);
						appPtr->offsetBottom.x()(1) = pix(3) - ((int)center(1) - params.patchSize);
						appPtr->offsetBottom.P() = jblas::zero_mat(2); // by definition this is our landmark projection

						ret = true;
					}
				}

				return ret;
         }

			void fillDataObs(const boost::shared_ptr<FeatureImageSegment> & featPtr, boost::shared_ptr<ObservationAbstract> & obsPtr)
         {
            // extract observed appearance
				app_img_seg_ptr_t app_src = SPTR_CAST<AppearanceImageSegment>(featPtr->appearancePtr);
				app_img_seg_ptr_t app_dst = SPTR_CAST<AppearanceImageSegment>(obsPtr->observedAppearance);
            app_dst->setHypothesis(app_src->hypothesis());
				app_src->patch.copyTo(app_dst->patch);
            /*app_src->patch.copy(app_dst->patch, (app_src->patch.width()-app_dst->patch.width())/2,
                  (app_src->patch.height()-app_dst->patch.height())/2, 0, 0,
                  app_dst->patch.width(), app_dst->patch.height());*/
				app_dst->offsetTop = app_src->offsetTop;
				app_dst->offsetBottom = app_src->offsetBottom;

            // create descriptor
            descriptor_ptr_t descPtr(descFactory->createDescriptor());
            obsPtr->landmarkPtr()->setDescriptor(descPtr);
         }

   };

}}

#endif //HAVE_MODULE_DSEG

#endif // RAWSEGPROCESSORS_HPP
