// Author(s): A.J. (Hannes) Pretorius
// Copyright: see the accompanying file COPYING or copy at
// https://github.com/mCRL2org/mCRL2/blob/master/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file ./examiner.h

#ifndef EXAMINER_H
#define EXAMINER_H

#include "diagram.h"
#include "settings.h"

class Examiner : public Visualizer
{
  Q_OBJECT

  public:
    // -- constructors and destructor -------------------------------
    Examiner(QWidget* parent, Settings* settings, Graph* graph);
    virtual ~Examiner();

    QColor selectionColor() { return VisUtils::coolRed; }
    std::size_t selectedClusterIndex();

    void setDiagram(Diagram* dgrm) { diagram = dgrm; update(); }
    void setFrame(Cluster* frme, const std::vector<Attribute*>& attrs, QColor col);
    void clearFrame();

    void addFrameHist(Cluster* frme, const std::vector<Attribute*>& attrs);
    void addFrameHist(QList<Cluster*> frames, const std::vector<Attribute*>& attrs);

    // -- visualization functions  ----------------------------------
    void visualize();
    void mark();

    // -- event handlers --------------------------------------------
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

    QSize sizeHint() const { return QSize(200,200); }

  protected slots:
    void clearData();
    void clearFrameHistCur();

  signals:
    void routingCluster(Cluster* cluster, QList<Cluster*> clusterSet, QList<Attribute*> attributes);
    void selectionChanged();

  private:
    // -- utility functions -----------------------------------------
    void calcSettingsGeomBased();
    void calcSettingsDataBased();
    void calcPosFrame();
    void calcPosFramesHist();

    void clearAttributes();
    void clearDiagram();
    void clearFrames();

    // -- hit detection ---------------------------------------------
    void handleIconRwnd();
    void handleIconLft();
    /*
    void handleIconPlay();
    */
    void handleIconRgt();

    // -- utility drawing functions ---------------------------------
    void clear();

    template <Mode> void drawFrame();
    template <Mode> void drawFramesHist();
    template <Mode> void drawControls();
    template <Mode> void draw();

    enum
    {
      ID_ICON_CLR,
      ID_ICON_MORE,
      ID_ICON_RWND,
      ID_ICON_LFT,
      ID_ICON_RGT,
      ID_ICON_PLAY,
      ID_FRAME,
      ID_FRAME_HIST
    };

    // -- data members ----------------------------------------------
    Settings* settings;

    Diagram* diagram;                // association
    std::vector< Attribute* > attributes; // association

    Cluster* frame;                  // composition
    QColor colFrm;

    std::vector< Cluster* > framesHist;            // composition
    std::vector< std::vector< Attribute* > > attrsHist; // association

    Position2D posFrame;
    std::vector< Position2D > posFramesHist;

    std::size_t focusFrameIdx;

    double scaleFrame;
    double scaleFramesHist;

    double offset;
    std::size_t vsblHistIdxLft;
    std::size_t vsblHistIdxRgt;
};

#endif
