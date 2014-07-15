/* auto_identify_plugin.cpp
 * This is a test plugin, you can use it as a demo.
 * 2012-01-01 : by YourName
 */
 
#include "v3d_message.h"
#include <vector>
#include <math.h>
#include "auto_identify_plugin.h"
#include "string"
#include "sstream"
#include "../../v3d_main/neuron_editing/v_neuronswc.h"
#include "../../v3d_main/neuron_tracing/neuron_tracing.h"
#include <time.h>

using namespace std;
Q_EXPORT_PLUGIN2(auto_identify, AutoIdentifyPlugin);

#define V_NeuronSWC_list vector<V_NeuronSWC>


void markers_singleChannel(V3DPluginCallback2 &callback, QWidget *parent);
void count_cells(V3DPluginCallback2 &callback, QWidget *parent);
void identify_neurons(V3DPluginCallback2 &callback, QWidget *parent);
template <class T> LandmarkList main_func(T* data1d, V3DLONG *dimNum, int c, LandmarkList & mlist, LandmarkList & bglist);
LandmarkList neuron_2_mark(const NeuronTree & p, LandmarkList & neuronMarkList);
template <class T> int pixelVal(T* data1d, V3DLONG *dimNum,
                                int xc, int yc, int zc, int c);
template <class T> LocationSimple mass_center(T* data1d,
                                              V3DLONG *dimNum,
                                              int xc, int yc, int zc, int rad, int c);
template <class T> pair<int,int> pixel(T* data1d,
                                        V3DLONG *dimNum,
                                        int xc,int yc,int zc,int c,int rad);
template <class T> pair<int,int> dynamic_pixel(T* data1d,
                                        V3DLONG *dimNum,
                                        int xc, int yc, int zc,
                                        int c, int PixVal, int BGVal);
template <class T> LandmarkList count(T* data1d,
                                      V3DLONG *dimNum,
                                      int MarkAve, int MarkStDev,
                                      int PointAve, int PointStDev,
                                      int rad, int radAve, int radStDev, int c, int cat);
template <class T> LandmarkList duplicates(T* data1d, LandmarkList fullList,
                                           V3DLONG *dimNum, int PointAve, int rad, int c);
V_NeuronSWC get_v_neuron_swc(const NeuronTree *p);
V_NeuronSWC_list get_neuron_segments(const NeuronTree *p);
//NeuronTree VSWClist_2_neuron_tree(V_NeuronSWC_list *p);
NeuronTree VSWC_2_neuron_tree(V_NeuronSWC *p, int id);
NeuronSWC make_neuron_swc(V_NeuronSWC_unit *p, int id, bool &start);
template <class T> double compute_radius(T* data1d, V3DLONG *dimNum, vector<V_NeuronSWC_unit> segment, int c);
bool export_list2file(QList<NeuronTree> & N2, QString fileSaveName, QString fileOpenName);

 
QStringList AutoIdentifyPlugin::menulist() const
{
	return QStringList() 
        <<tr("Single Channel Cell Counting")
        <<tr("Better Cell Counting")
        <<tr("Neurons")
		<<tr("about");
}

QStringList AutoIdentifyPlugin::funclist() const
{
	return QStringList()
		<<tr("func1")
		<<tr("func2")
		<<tr("help");
}

void AutoIdentifyPlugin::domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent)
{
    if (menu_name == tr("Single Channel Cell Counting"))
	{
        //markers_singleChannel(callback,parent);
        v3d_msg("Please use the other one, this one is outdated and probably full of bugs. Sorry :<");
	}
    else if (menu_name == tr("Better Cell Counting"))
	{
        count_cells(callback,parent);
	}
    else if (menu_name == tr("Neurons"))
    {
        //v3d_msg("To be implemented"); return;
        identify_neurons(callback,parent);
    }
	else
	{
        v3d_msg(tr("Uses current image's landmarks to find similar objects in image."
            "Work-in-process by Xindi, 2014 Summer"));
	}
}


bool AutoIdentifyPlugin::dofunc(const QString & func_name, const V3DPluginArgList & input, V3DPluginArgList & output, V3DPluginCallback2 & callback,  QWidget * parent)
{
	vector<char*> infiles, inparas, outfiles;
	if(input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
	if(input.size() >= 2) inparas = *((vector<char*> *)input.at(1).p);
	if(output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);

	if (func_name == tr("func1"))
	{
		v3d_msg("To be implemented.");
	}
	else if (func_name == tr("func2"))
	{
		v3d_msg("To be implemented.");
	}
	else if (func_name == tr("help"))
	{
		v3d_msg("To be implemented.");
	}
	else return false;

	return true;
}

/*  ##################################
 * [completed tasks]
 * test neuron SWC needs to be its own structure marked with the comment "test"
 * radius calculation of branches works
 * averate intensity of branches works
 * categorizing non-test data sets based on test data works
 * saving newly labeled SWC file works
 *
 * [current goals/issues]
 * updating window to reflect type changes does not work yet
 * add in consistency of intensity as potential type identifiers
 *      because the radius calculation is currently skipping dark spots, darker neurons may be classified as brighter than they should be
 *
 * [future goals]
 * find way to identify individal segments rather than entire structures as test data. NOT SURE IF EVEN POSSIBLE
 * optimize radius calculation algorithm to be more robust
 *  ##################################
*/
void identify_neurons(V3DPluginCallback2 &callback, QWidget *parent)
{
    v3dhandle curwin = callback.currentImageWindow();

    //cancels if no image
    if (!curwin)
    {
        v3d_msg("You don't have any image open in the main window.");
        return;
    }

    //if image, pulls the data
    Image4DSimple* p4DImage = callback.getImage(curwin); //the data of the image is in 4D (channel + 3D)

    unsigned char* data1d = p4DImage->getRawData(); //sets data into 1D array

    QString curfile = callback.getImageName(curwin);

    //defining the dimensions
    V3DLONG N = p4DImage->getXDim();
    V3DLONG M = p4DImage->getYDim();
    V3DLONG P = p4DImage->getZDim();
    V3DLONG sc = p4DImage->getCDim();

    //storing the dimensions
    V3DLONG dimNum[4];
    dimNum[0]=N; dimNum[1]=M; dimNum[2]=P; dimNum[3]=sc;

    //input channel
    unsigned int c=1;
    bool ok;
    if (sc==1)
        c=1; //if only using 1 channel
    else
        c = QInputDialog::getInteger(parent, "Channel", "Enter Channel Number", 1, 1, sc, 1,&ok);

    QList<NeuronTree> * mTreeList;
    mTreeList = callback.getHandleNeuronTrees_3DGlobalViewer(curwin);
    NeuronTree mTree;
    if (mTreeList->isEmpty()) { v3d_msg("There are no neuron traces in the current window."); return; }
    else
    {
        vector<int> segCatArr;
        vector<double> segRadArr;
        vector<double> segIntensArr;
        int structNum = mTreeList->count();

        //get radius and type from test data
        for (int i=0; i<structNum; i++)
        {
            mTree = mTreeList->at(i);

            if (mTree.comment != "test") continue; //defining the testing set by comments

            V_NeuronSWC_list seg_list = get_neuron_segments(&mTree);
            //syntax: list.at(i) is segment, segment.row is vector of units, vector.at(i) is unit, unit.type is category
            int segNum = seg_list.size();
            //v3d_msg(QString("read in tree with %1 segments").arg(segNum));
            for (int j=0; j<segNum; j++)
            {
                segCatArr.push_back(seg_list.at(j).row.at(0).type);
                double radAve = compute_radius(data1d,dimNum,seg_list.at(j).row,c);
                double x,y,z,intensity=0;
                for (int k=0; k<seg_list.at(j).row.size(); k++)
                {
                    x=seg_list.at(j).row.at(k).x;
                    y=seg_list.at(j).row.at(k).y;
                    z=seg_list.at(j).row.at(k).z;
                    intensity += pixelVal(data1d,dimNum,x,y,z,c);
                }
                double intensAve = intensity/seg_list.at(j).row.size();
                segRadArr.push_back(radAve);
                segIntensArr.push_back(intensAve);
                //v3d_msg(QString("cat %1 rad %2").arg(seg_list.at(j).row.at(0).type).arg(radAve));
            }
        }
        //v3d_msg(QString("%1 %2 %3 %4").arg(segCatArr.size()).arg(segCatArr.at(0)).arg(segRadArr.size()).arg(segRadArr.at(0)));

        //label remaining neurons using test data
        NeuronTree newTree;
        QList<NeuronTree> newTreeList;
        for (int i=0; i<structNum; i++) //loops through neuron structures as numbered in object manager
        {
            mTree = mTreeList->at(i);

            if (mTree.comment == "test") continue; //defining the testing set by comments

            V_NeuronSWC_list seg_list = get_neuron_segments(&mTree);
            //syntax: list.at(i) is segment, segment.row is vector of units, vector.at(i) is unit, unit.type is category
            int segNum = seg_list.size();
            int id=1;
            //v3d_msg(QString("read in segment of length %1").arg(segNum));
            for (int j=0; j<segNum; j++) //loops through segments within one structure
            {
                double radAve = compute_radius(data1d,dimNum,seg_list.at(j).row,c);
                double x,y,z,intensity=0;
                for (int k=0; k<seg_list.at(j).row.size(); k++)
                {
                    x=seg_list.at(j).row.at(k).x;
                    y=seg_list.at(j).row.at(k).y;
                    z=seg_list.at(j).row.at(k).z;
                    intensity += pixelVal(data1d,dimNum,x,y,z,c);
                }
                double intensAve = intensity/seg_list.at(j).row.size();
                //v3d_msg(QString("radius %1").arg(radAve));
                double diffRad,diffInt,diff,diff_min=255;
                int cur_type=3;
                for (int k=0; k<segRadArr.size(); k++)
                {
                    diffRad = abs(radAve-segRadArr.at(k));
                    diffInt = abs(intensAve-segIntensArr.at(k));
                    diff = (diffRad+diffInt)/2; //depending on further testing, may end up weighing this average differently
                    //v3d_msg(QString("diff %3 between calculated %1 and test %2").arg(radAve).arg(segRadArr.at(k)).arg(diff));
                    if (diff<diff_min)
                    {
                        cur_type=segCatArr.at(k);
                        diff_min=diff;
                    }
                }
                //v3d_msg(QString("identified as type %1").arg(cur_type));
                for (int l=0; l<seg_list.at(j).row.size(); l++)
                {
                    seg_list.at(j).row.at(l).type=cur_type; //sets every unit in segment to be new type
                }
                //attempting to draw in new updated neuron trees, not working...
                newTree = VSWC_2_neuron_tree(&seg_list.at(j),id); //translates segment into a NeuronTree
                id += seg_list.at(j).row.size();
                //callback.setSWC(curwin,newTree);
                //mTreeList->replace(i,newTree);
                newTreeList.append(newTree);
                //v3d_msg(QString("changed segment %1 of rad %3 to type %2").arg(j).arg(cur_type).arg(radAve));
            }

        }
        //need to draw newTreeList into window and remove mTreeList
        //*mTreeList = newTreeList;
        //callback.updateImageWindow(curwin);
        export_list2file(newTreeList,"Labeled_SWC.swc",curfile);
    }

    return;
}


/*  ##################################
 * [completed tasks]
 * all main algorithms are functional, may not be optimized
 *
 * [current goals/issues]
 * mass_center tends to skew markers towards (0,0,0), compromises test data calculations
 * pixel function has two algorithms, the first works but is slow, the second is faster but returns bad data
 *
 * [future goals]
 * include overload for when test data includes no background markers
 *  ##################################
*/
void count_cells(V3DPluginCallback2 &callback, QWidget *parent)
{
    v3dhandle curwin = callback.currentImageWindow();

    //cancels if no image
    if (!curwin)
    {
        v3d_msg("You don't have any image open in the main window.");
        return;
    }

    //if image, pulls the data
    Image4DSimple* p4DImage = callback.getImage(curwin); //the data of the image is in 4D (channel + 3D)

    unsigned char* data1d = p4DImage->getRawData(); //sets data into 1D array

    //defining the dimensions
    V3DLONG N = p4DImage->getXDim();
    V3DLONG M = p4DImage->getYDim();
    V3DLONG P = p4DImage->getZDim();
    V3DLONG sc = p4DImage->getCDim();

    //storing the dimensions
    V3DLONG dimNum[4];
    dimNum[0]=N; dimNum[1]=M; dimNum[2]=P; dimNum[3]=sc;

    LandmarkList Marklist = callback.getLandmark(curwin);
    int Marknum = Marklist.count();
    QList<NeuronTree> * mTreeList;
    mTreeList = callback.getHandleNeuronTrees_3DGlobalViewer(curwin);
    int SWCcount;
    NeuronTree mTree;
    if (mTreeList->isEmpty()) { SWCcount = 0; }
    else
    {
        mTree = mTreeList->first();
        SWCcount = mTree.listNeuron.count();
    }
    //NeuronTree mTree = callback.getSWC(curwin);
    //int SWCcount = mTree.listNeuron.count();

    //input test data type
    int option;
    if (Marknum != 0 && SWCcount ==0 ) { option = 1; }
    else if (Marknum == 0 && SWCcount != 0) { option = 2; }
    else
    {
        QString qtitle = QObject::tr("Choose Test Data Input Type");
        bool ok;
        QStringList items;
        items << "Markers" << "3D Curves" << "Markers and 3D Curves";
        QString item = QInputDialog::getItem(0, qtitle,
                                            QObject::tr("Which type of testing data are you using"), items, 0, false, &ok);
        if (! ok) return;
        int input_type = items.indexOf(item);
        if (input_type==0) { option = 1; }
        else if (input_type==1){ option = 2; }
        else { option = 3; }
    }

    LandmarkList mlist, neuronMarkList;
    if (option == 1)
    {
        mlist = Marklist;
    }
    else if (option == 2 )
    {
        neuronMarkList = neuron_2_mark(mTree,neuronMarkList);
        mlist = neuronMarkList;
    }
    else
    {
        neuronMarkList = neuron_2_mark(mTree,neuronMarkList);
        mlist = Marklist;
        mlist.append(neuronMarkList);
    }

    bool ok;

    //input channel
    unsigned int c=1;
    if (sc==1)
        c=1; //if only using 1 channel
    else
        c = QInputDialog::getInteger(parent, "Channel", "Enter Channel Number", 1, 1, sc, 1,&ok);


    //input sort method
    QString qtitle = QObject::tr("Choose Sorting Method");
    QStringList items;
    items << "Default (binary color threshold)" << "By Type";
    QString item = QInputDialog::getItem(0, qtitle,
                                         QObject::tr("How should the test data be sorted?"), items, 0, false, &ok);
    if (! ok) return;
    int input_type = items.indexOf(item);
    if (input_type==0) //default
    {
        LandmarkList bglist; //sending in empty bglist to trigger binary sort
        LandmarkList smallList = main_func(data1d,dimNum,c,mlist,bglist);
        LandmarkList& woot2 = smallList;
        bool draw_le_markers2 = callback.setLandmark(curwin,woot2);
    }
    else if (input_type==1) //type
    {
        //int catNum = QInputDialog::getInt(0,"Number of Categories","Enter number of categories (background category included), if unsure enter 0",0,0,100,1,&ok);
        //Can't think of reason having user inputed cat number would be better than auto counting

        int * catList;
        catList = new int[mlist.count()];
        if (mlist.count()==0) {v3d_msg("There are no neuron traces in the current image"); return;}
        LocationSimple tempInd;
        for (int i=0; i<mlist.count(); i++)
        {
            tempInd = mlist.at(i);
            catList[i] = tempInd.category;
//            v3d_msg((QString("hi %1, cat %2").arg(i).arg(catList[i])));
        }

        //counts number of categories
        int catNum=0;
        for (int i=0; i<mlist.count(); i++)
        {
            for (int j=0; j<mlist.count(); j++)
            {
                if (catList[j]==i)
                {
                    catNum++;
                    break;
                }
            }
        }

//v3d_msg(QString("final catNum %1").arg(catNum));


//v3d_msg("start indexing");
        map<int,LandmarkList> catArr;
        LandmarkList temp;
        int row=0;
        for (int catval=0; catval<mlist.count(); catval++) //loop through category values
        {
            int x=0;
            for (int index=0; index<mlist.count(); index++) //loop through markers
            {
                if (catList[index]==catval)
                {
                    x++;
                    temp.append(mlist.at(index));
                }
            }
//            v3d_msg(QString("found %1 values for catVal %2").arg(x).arg(catval));
            if (x==0)
                continue;
            else
            {
                catArr.insert(make_pair(row,temp));
//v3d_msg(QString("row %1 cat %2").arg(row).arg(temp.at(0).category));
                row++;
                temp.clear();
            }
        }
        if (catList) {delete [] catList; catList=0;}

//        v3d_msg("indexing complete");


//v3d_msg("arrays made");
        //run script
        LandmarkList catSortList;
        LandmarkList * marks;
        LandmarkList * bgs = &catArr[0]; //working with assumption that bg has category value 0;
        for (int i=0; i<catNum-1; i++)
        {
//v3d_msg(QString("catSortList start iteration %1").arg(i));
            /*LandmarkList marksL;
            for (int j=0; i<catInd[i+1]; j++)
            {
                marksL.append(mlist.at(catArr[i+1][j]));
            }
            marks = &marksL;*/
            marks = &catArr[i+1];
//v3d_msg(QString("marks %1").arg(marks->count()));
            LandmarkList tempList = main_func(data1d,dimNum,c,*marks,*bgs);
            catSortList.append(tempList);
//v3d_msg(QString("catSortList append category %1").arg(tempList.at(0).category));
//            marksL.clear();
        }

        LandmarkList& woot3 = catSortList;
        bool draw_le_markers3 = callback.setLandmark(curwin,woot3);

    }
    return;
}

template <class T> LandmarkList main_func(T* data1d, V3DLONG *dimNum, int c, LandmarkList & markerlist, LandmarkList & bglist)
{
    LandmarkList mlist, MarkList, BGList;
    LocationSimple tmpLocation(0,0,0);
    int xc,yc,zc, marks;
    double PixVal,BGVal;
    if (bglist.isEmpty()) //binary sorting
    {
        //sort markers by background/foreground
        mlist = markerlist;
//v3d_msg("presort ckpt");

        int pix,num;
        int marknum = mlist.count();
        int * PixValArr;
        PixValArr = new int[marknum];
//v3d_msg("sort ckpt 1");
        for (int i=0; i<marknum; i++)
        {
            tmpLocation = mlist.at(i);
            tmpLocation.getCoord(xc,yc,zc);
            pix = pixelVal(data1d,dimNum,xc,yc,zc,c);
            //      v3d_msg(QString("pix value %1 %2").arg(pix).arg(pix1));
            PixValArr[i] = pix;
        }
        int max=0,min=255;
        for (int i=0; i<marknum; i++)
        {
            num=PixValArr[i];
            if (num>max) { max=num; }
            if (num<min) { min=num; }
        }
//v3d_msg(QString("sort ckpt 2, min %1 max %2").arg(min).arg(max));
        int thresh = (max+min)/2;
        PixVal=0, BGVal=0;
        for (int i=0; i<marknum; i++)
        {
            num=PixValArr[i];
            tmpLocation = mlist.at(i);
            if (num<thresh) { BGList.append(tmpLocation); BGVal += num; }    //BGList holds bg markers
            if (num>thresh) { MarkList.append(tmpLocation); PixVal += num; }  //MarkList holds cell markers
        }
    }
    else    //comment sorting
    {
        MarkList = markerlist;
        PixVal=0, BGVal=0;
        for (int i=0; i<MarkList.count(); i++)
        {
            tmpLocation = MarkList.at(i);
            tmpLocation.getCoord(xc,yc,zc);
            int pix = pixelVal(data1d,dimNum,xc,yc,zc,c);
            PixVal += pix;
        }
        BGList = bglist;
        for (int i=0; i<BGList.count(); i++)
        {
            tmpLocation = BGList.at(i);
            tmpLocation.getCoord(xc,yc,zc);
            int pix = pixelVal(data1d,dimNum,xc,yc,zc,c);
            BGVal += pix;
        }
    }

    marks = MarkList.count();
    PixVal = PixVal/marks;          //PixVal now stores average pixel value of all cell markers
    BGVal = BGVal/BGList.count();   //BGVal now stores average pixel value of all background markers
    int cat = MarkList.at(0).category;
//    v3d_msg(QString("PixVal %1, pixCount %2, BGVal %3, BGCount %4").arg(PixVal).arg(marks).arg(BGVal).arg(BGList.count()));


//v3d_msg(QString("marks = %1, bgcount = %2. Marks all sorted").arg(marks).arg(BGList.count()));


    //recalibrates marker list by mean shift
    LandmarkList tempList;
    LocationSimple temp(0,0,0),newMark(0,0,0);
    //int tempPix;
    for (int i=0; i<marks; i++)
    {
        temp = MarkList.at(i);
        temp.getCoord(xc,yc,zc);
        //tempPix = pixelVal(data1d,dimNum,xc,yc,zc,c);
        for (int j=0; j<10; j++)
        {
            newMark = mass_center(data1d,dimNum,xc,yc,zc,15,c);
            newMark.getCoord(xc,yc,zc);
        }
        tempList.append(newMark);
    }
    MarkList = tempList;
//    return MarkList;

    //scan list of cell markers for ValAve, radAve

    int * ValAveArr; int * radAveArr;
    ValAveArr = new int[marks]; radAveArr = new int[marks];
    LocationSimple tempLocation(0,0,0);
    double ValAve=0,radAve=0;
    clock_t t1;
    t1 = clock();
    for (int i=0; i<marks; i++)
    {
        tempLocation = MarkList.at(i);
        tempLocation.getCoord(xc,yc,zc);
        int Pix = pixelVal(data1d,dimNum,xc,yc,zc,c);

        pair<int,int> dynAns = dynamic_pixel(data1d,dimNum,xc,yc,zc,c,Pix,BGVal);
        ValAveArr[i] = dynAns.first;
        radAveArr[i] = dynAns.second;
        ValAve += ValAveArr[i];
        radAve += radAveArr[i];

//v3d_msg(QString("ValAve %1, radAve %2").arg(ValAve).arg(radAve));
    }
    t1 = clock() - t1;
    cout<<"dynamic pixel time "<<t1<<endl;

//v3d_msg("scan checkpoint");

    ValAve /= marks;  //average pixel value of each segment
    radAve /= marks;  //average radius of segment

//v3d_msg(QString("FINAL ValAve %1, radAve %2").arg(ValAve).arg(radAve));

    double stV=0, stR=0, stP=0;
    for (int i=0; i<marks; i++)
    {
        double s = (ValAveArr[i]-ValAve)*(ValAveArr[i]-ValAve);
        stV += s;

        double t = (radAveArr[i]-radAve)*(radAveArr[i]-radAve);
        stR += t;

        tempLocation = MarkList.at(i);
        tempLocation.getCoord(xc,yc,zc);
        int Pix = pixelVal(data1d,dimNum,xc,yc,zc,c);
//        v3d_msg(QString("Pix %1 and PixVal %2").arg(Pix).arg(PixVal));
        double u = (Pix-PixVal)*(Pix-PixVal);
        stP += u;
//v3d_msg(QString("pixel value %1, diff %2, stP %3").arg(Pix).arg(Pix-PixVal).arg(stP));
    }
    double ValStDev = sqrt(stV/marks);
    double radStDev = sqrt(stR/marks);
    double PixStDev = sqrt(stP/marks);


//v3d_msg(QString("markers have been scanned. Pixval %1 and stdev %2. radVal %3 and stdev %4."
//                      "segVal %5 and stdev %6").arg(PixVal).arg(PixStDev).arg(radAve).arg(radStDev).arg(ValAve).arg(ValStDev));
v3d_msg(QString("category %1").arg(cat));
    clock_t t;
    t = clock();
    //scans image and generates new set of markers based on testing data
    LandmarkList newList = count(data1d,dimNum,ValAve,2*ValStDev,PixVal,PixStDev,0,radAve,5+radStDev,c,cat);
    t = clock() - t;
    cout<<"count time "<<t<<endl;

    //recenters list via mean shift
v3d_msg("recentering");
    LandmarkList tempL2;
    LocationSimple temp2;
    for (int i=0; i<newList.count(); i++)
    {
        temp2 = newList.at(i);
        temp2.getCoord(xc,yc,zc);
        for (int j=0; j<10; j++)
        {
            newMark = mass_center(data1d,dimNum,xc,yc,zc,radAve,c);
            newMark.getCoord(xc,yc,zc);
            newMark.category = cat;
            stringstream catStr;
            catStr << cat;
            newMark.comments = catStr.str();
        }
        tempL2.append(newMark);
    }
//v3d_msg("ckpt 2");
    newList = tempL2;

//v3d_msg("newList made");


    //        LandmarkList& woot = newList;
    //        bool draw_le_markers = callback.setLandmark(curwin,woot);
    //        v3d_msg(QString("newList has %1 markers").arg(newList.count()));

    //deletes duplicate markers based on their proximity
    LandmarkList smallList = duplicates(data1d,newList,dimNum,PixVal,radAve,c);
//v3d_msg("duplicates deleted");

    return smallList;
}




//returns pixel value of marker
template <class T> int pixelVal(T* data1d, V3DLONG *dimNum,
                                int xc, int yc, int zc, int c)
{
    V3DLONG N = dimNum[0];
    V3DLONG M = dimNum[1];
    V3DLONG P = dimNum[2];
    V3DLONG shiftC = (c-1)*P*M*N;
    int pixelVal = data1d[ shiftC + (V3DLONG)zc*M*N + (V3DLONG)yc*N + (V3DLONG)xc ];
    return pixelVal;
}


//returns new marker that has been recentered
template <class T> LocationSimple mass_center(T* data1d,
                                              V3DLONG *dimNum,
                                              int xc, int yc, int zc, int rad, int c)
{
    V3DLONG N = dimNum[0];
    V3DLONG M = dimNum[1];
    V3DLONG P = dimNum[2];

    //int min=255,newX=0,newY=0,newZ=0;
    //int xweight=0,yweight=0,zweight=0,kernel=0,ktot=0;
    double pVal;
    rad=5;

    //defining limits
    V3DLONG xLow = xc-rad; if(xLow<0) xLow=0;
    V3DLONG xHigh = xc+rad; if(xHigh>N-1) xHigh=N-1;
    V3DLONG yLow = yc-rad; if(yLow<0) yLow=0;
    V3DLONG yHigh = yc+rad; if(yHigh>M-1) yHigh=M-1;
    V3DLONG zLow = zc-rad; if(zLow<0) zLow=0;
    V3DLONG zHigh = zc+rad; if(zHigh>P-1) zHigh=P-1;

    //scanning through the pixels
    double newX=0, newY=0, newZ=0, norm=0;
    V3DLONG k,j,i;
    for (k = zLow; k <= zHigh; k++)
    {
         for (j = yLow; j <= yHigh; j++)
         {
             for (i = xLow; i <= xHigh; i++)
             {
                 double t = (i-xc)*(i-xc)+(j-yc)*(j-yc)+(k-zc)*(k-zc);
                 double dist = sqrt(t);
                 if (dist<=rad)
                 {
                     pVal = pixelVal(data1d,dimNum,i,j,k,c);
                     newX += pVal*i;
                     newY += pVal*j;
                     newZ += pVal*k;
                     norm += pVal;
                 }
             }
         }
    }

    newX /= norm;
    newY /= norm;
    newZ /= norm;

    LocationSimple newMark(newX,newY,newZ);
//    v3d_msg(QString("New coords %1 %2 %3 vs old coords %4 %5 %6").arg(newX).arg(newY).arg(newZ).arg(xc).arg(yc).arg(zc));
    return newMark;
}


//returns average pixel value and radius of cell around a marker
template <class T> pair<int,int> dynamic_pixel(T* data1d,
                                        V3DLONG *dimNum,
                                        int xc, int yc, int zc,
                                        int c, int PixVal, int BGVal)
{
    V3DLONG N = dimNum[0];
    V3DLONG M = dimNum[1];
    V3DLONG P = dimNum[2];
    V3DLONG shiftC = (c-1)*P*M*N;

    int rad=0,dataAve;
    do
    {
        rad++;
        //defining limits
        V3DLONG xLow = xc-rad; if(xLow<0) xLow=0;
        V3DLONG xHigh = xc+rad; if(xHigh>N-1) xHigh=N-1;
        V3DLONG yLow = yc-rad; if(yLow<0) yLow=0;
        V3DLONG yHigh = yc+rad; if(yHigh>M-1) yHigh=M-1;
        V3DLONG zLow = zc-rad; if(zLow<0) zLow=0;
        V3DLONG zHigh = zc+rad; if(zHigh>P-1) zHigh=P-1;

        //scanning through the pixels
        V3DLONG k,j,i;
        //average data of each segment
        int datatotal=0,runs=0;
        for (k = zLow; k <= zHigh; k++)
        {
            V3DLONG shiftZ = k*M*N;
            for (j = yLow; j <= yHigh; j++)
            {
                V3DLONG shiftY = j*N;
                for (i = xLow; i <= xHigh; i++)
                {
                    double t = (i-xc)*(i-xc)+(j-yc)*(j-yc)+(k-zc)*(k-zc);
                    double dist = sqrt(t);
                    if (dist<=rad)
                    {
                        int dataval = data1d[ shiftC + shiftZ + shiftY + i ];
                        datatotal += dataval;
                        runs++;
                    }
                }
            }
        }
        dataAve = datatotal/runs;
    } while ( dataAve > (PixVal+2*BGVal)/3 );

    return make_pair(dataAve,rad);
}


//returns average pixel value in box of radius rad on channel c around a given marker as well as average cell marker intensity
template <class T> pair<int,int> pixel(T* data1d,
                              V3DLONG *dimNum,
                              int xc,int yc,int zc,int c,int rad)
{
    V3DLONG N = dimNum[0];
    V3DLONG M = dimNum[1];
    V3DLONG P = dimNum[2];

    V3DLONG shiftC = (c-1)*P*M*N;

    //defining limits
    V3DLONG xLow = xc-rad; if(xLow<0) xLow=0;
    V3DLONG xHigh = xc+rad; if(xHigh>N-1) xHigh=N-1;
    V3DLONG yLow = yc-rad; if(yLow<0) yLow=0;
    V3DLONG yHigh = yc+rad; if(yHigh>M-1) yHigh=M-1;
    V3DLONG zLow = zc-rad; if(zLow<0) zLow=0;
    V3DLONG zHigh = zc+rad; if(zHigh>P-1) zHigh=P-1;

    //scanning through the pixels
    V3DLONG k,j,i;
    //average data of each segment
    int datatotal=0,runs=0;
    double t,dist;
    for (k = zLow; k <= zHigh; k++)
    {
         V3DLONG shiftZ = k*M*N;
         for (j = yLow; j <= yHigh; j++)
         {
             V3DLONG shiftY = j*N;
             for (i = xLow; i <= xHigh; i++)
             {
                 t = (i-xc)*(i-xc)+(j-yc)*(j-yc)+(k-zc)*(k-zc);
                 dist = sqrt(t);
                 if (dist<=rad)
                 {
                     int dataval = data1d[ shiftC + shiftZ + shiftY + i ];
                     datatotal += dataval;
                     runs++;
                 }
                 else continue;
             }
         }
    }

    /*double x,y,z,datatotal,pi=3.14;
    int runs=0;
    if (rad==0); rad=1;
    double radinterval = rad;
    radinterval/=5;
    for (double r=0; r<rad; r+=radinterval) //r isn't increasing
    {
        for (double theta=0; theta<2*pi; theta+=(pi/4))
        {
            for (double phi=0; phi<pi; phi+=(pi/4))
            {
                //cout<<"pixel iteration "<<runs<<endl;
                //cout<<r<<" "<<theta<<" "<<phi<<endl<<endl;
                x = xc+r*cos(theta)*sin(phi);
                y = yc+r*sin(theta)*sin(phi);
                z = zc+r*cos(phi);
                double dataval = pixelVal(data1d,dimNum,x,y,z,c);
                datatotal += dataval;
                runs++;
            }
        }
    }*/
    int dataAve = datatotal/runs;
    //data of point
    int pointval = pixelVal(data1d,dimNum,xc,yc,zc,c);
    //cout<<dataAve<<endl;

    return make_pair(dataAve,pointval);
}


//uses test data to scan and mark other cells
template <class T> LandmarkList count(T* data1d,
                              V3DLONG *dimNum,
                              int MarkAve, int MarkStDev,
                              int PointAve, int PointStDev,
                              int rad, int radAve, int radStDev, int c, int cat)
{
    V3DLONG N = dimNum[0];
    V3DLONG M = dimNum[1];
    V3DLONG P = dimNum[2];
    //1D data array stores in this order: C Z Y X

    LocationSimple tmpLocation(0,0,0);
    LandmarkList newList;
    int seg;

    //this part is for user-entered rad
    if (rad!=0)
    {
        seg=rad/2;
        for (V3DLONG iz=seg; iz<P; iz+=seg)
        {
            for (V3DLONG iy=seg; iy<M; iy+=seg)
            {
                for (V3DLONG ix=seg; ix<N; ix+=seg)
                {
                    //(ix,iy,iz,c) are the coords that we are currently at
                    //we throw these coords into func pixel to get the pixel value to compare to the training values
                    //both sets of averages and st devs have to match up
                    pair<int,int> check = pixel(data1d,dimNum,ix,iy,iz,c,rad);
                    //we will say for now there is a cell if the test data is within 1 std of the training data
                    int TempDataAve = check.first;
                    int TempPointAve = check.second;
                    if ( (TempPointAve>=PointAve-2*PointStDev) && (TempPointAve<=PointAve+2*PointStDev))
                    {
                        if ( (TempDataAve>=MarkAve-MarkStDev) && (TempDataAve<=MarkAve+MarkStDev) )
                        {
                            tmpLocation.x = ix;
                            tmpLocation.y = iy;
                            tmpLocation.z = iz;
                            tmpLocation.category = cat;
                            stringstream catStr;
                            catStr << cat;
                            tmpLocation.comments = catStr.str();
                            newList.append(tmpLocation);
                            continue;

                        }
                    }
                }
            }
        }
    }

    //this part is for dynamically calculated rad
    else
    {
        cout<<"starting count"<<endl;
        seg = radAve/2;
        int init;
        if ((radAve-radStDev) < 1) { init=1;}
        else { init=radAve-radStDev; }
        for (int i=init; i<=radAve+radStDev; i++)
        {
            for (V3DLONG iz=seg; iz<=P; iz+=seg)
            {
                for (V3DLONG iy=seg; iy<=M; iy+=seg)
                {
                    for (V3DLONG ix=seg; ix<=N; ix+=seg)
                    {
                        //(ix,iy,iz,c) are the coords that we are currently at
                        //checking radius i
                        //we throw these coords into func pixel to get the pixel value to compare to the training values
                        //both sets of averages and st devs have to match up
                        pair<int,int> check = pixel(data1d,dimNum,ix,iy,iz,c,i);
                        //we will say for now there is a cell if the test data is within 2 std of the training data
                        int TempDataAve = check.first;
                        int TempPointAve = check.second;
//v3d_msg(QString("%1 %2 %3").arg(i).arg(TempDataAve).arg(TempPointAve));
                        if ( (TempPointAve>=PointAve-2*PointStDev) && (TempPointAve<=PointAve+2*PointStDev) && (TempDataAve>=MarkAve-2*MarkStDev) && (TempDataAve<=MarkAve+2*MarkStDev))
                        {
                            tmpLocation.x = ix;
                            tmpLocation.y = iy;
                            tmpLocation.z = iz;
                            tmpLocation.category = cat;
                            stringstream catStr;
                            catStr << cat;
                            tmpLocation.comments = catStr.str();
                            newList.append(tmpLocation);
                            continue;
                        }
                        else continue;
                    }
                }
            } cout<<"rad iteration "<<i<<endl;
        }

    }

    //note this function does not remember where the test data actually was, so it should find them again
    return newList;
}


//detects markers too close together, deletes marker with pixel value farther from PointAve
template <class T> LandmarkList duplicates(T* data1d, LandmarkList fullList,
                                           V3DLONG *dimNum, int PointAve, int rad, int c)
{
    int marknum = fullList.count();
    LandmarkList smallList = fullList;
    int x1,y1,z1,x2,y2,z2,data1,data2;
    double t, dist;
    LocationSimple point1(0,0,0), point2(0,0,0);
    LocationSimple zero(0,0,0);
    LocationSimple& zer = zero; //zer is address of LocationSimple zero
    for (int i=0; i<marknum; i++)
    {
        for (int j=i+1; j<marknum; j++)
        {
            point1 = smallList.at(i);
            point1.getCoord(x1,y1,z1);
            int pix1 = pixelVal(data1d,dimNum,x1,y1,z1,c);
            point2 = smallList.at(j);
            point2.getCoord(x2,y2,z2);
            int pix2 = pixelVal(data1d,dimNum,x2,y2,z2,c);

            t = (x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)+(z1-z2)*(z1-z2);
            dist = sqrt(t);
            if (dist<rad)
            {
                data1 = abs(pix1-PointAve);
                data2 = abs(pix2-PointAve);

                if (data1>data2)
                    smallList.replace(i,zer) ;//replace point1 with 0 to avoid changing length of list and messing up indexes
                else
                    smallList.replace(j,zer) ;//replace point2
            }
        }
    }
    smallList.removeAll(zer);
    return smallList;
}

LandmarkList neuron_2_mark(const NeuronTree & p, LandmarkList & neuronMarkList)
{
    LocationSimple tmpMark(0,0,0);
    for (int i=0;i<p.listNeuron.size();i++)
    {
        tmpMark.x = p.listNeuron.at(i).x;
        tmpMark.y = p.listNeuron.at(i).y;
        tmpMark.z = p.listNeuron.at(i).z;
        tmpMark.category = p.listNeuron.at(i).type;
        neuronMarkList.append(tmpMark);
    }
    return neuronMarkList;
}

V_NeuronSWC get_v_neuron_swc(const NeuronTree *p)
{
    V_NeuronSWC cur_seg;	cur_seg.clear();
    const QList<NeuronSWC> & qlist = p->listNeuron;

    for (V3DLONG i=0;i<qlist.size();i++)
    {
        V_NeuronSWC_unit v;
        v.n		= qlist[i].n;
        v.type	= qlist[i].type;
        v.x 	= qlist[i].x;
        v.y 	= qlist[i].y;
        v.z 	= qlist[i].z;
        v.r 	= qlist[i].r;
        v.parent = qlist[i].pn;

        cur_seg.append(v);
        //qDebug("%d ", cur_seg.nnodes());
    }
    cur_seg.name = qPrintable(QString("%1").arg(1));
    cur_seg.b_linegraph=true; //donot forget to do this
    return cur_seg;
}
V_NeuronSWC_list get_neuron_segments(const NeuronTree *p)
{
    V_NeuronSWC cur_seg = get_v_neuron_swc(p);
    V_NeuronSWC_list seg_list;
    seg_list = cur_seg.decompose();
    return seg_list;
}
NeuronSWC make_neuron_swc(V_NeuronSWC_unit *p, int id, bool &start)
{
    NeuronSWC N;

    N.n     = id;
    N.type  = p->type;
    N.x     = p->x;
    N.y     = p->y;
    N.z     = p->z;
    N.r     = p->r;
    if (start==false)    N.parent = id-1;
    else                 N.parent = -1;

    return N;
}

/*NeuronTree VSWClist_2_neuron_tree(V_NeuronSWC_list *p)
{
    QList<NeuronSWC> nTree;
    for (int i=0; i<p->size(); i++)
    {
        V_NeuronSWC v = p->at(i);
        for (int j=0; j<v.row.size(); j++)
        {
            V_NeuronSWC_unit v2 = v.row.at(j);
            NeuronSWC n = make_neuron_swc(&v2);
            nTree.append(n);
        }
    }
    NeuronTree newTree;
    newTree.listNeuron = nTree;
    return newTree;
}*/
NeuronTree VSWC_2_neuron_tree(V_NeuronSWC *p, int id)
{
    QList<NeuronSWC> nTree;
    bool start;
    for (int j=0; j<p->row.size(); j++)
    {
        if (j==0)   start=true;
        else        start=false;
        V_NeuronSWC_unit v = p->row.at(j);
        NeuronSWC n = make_neuron_swc(&v,id,start);
        nTree.append(n);
        id++;
    }

    NeuronTree newTree;
    newTree.listNeuron = nTree;
    return newTree;
}

template <class T> double compute_radius(T* data1d, V3DLONG *dimNum, vector<V_NeuronSWC_unit> segment, int c)
{
    V3DLONG N = dimNum[0];
    V3DLONG M = dimNum[1];
    V3DLONG P = dimNum[2];
    double radAve;

    /*if (segment.size()>2)
    {
        double radTot=0;
        double k,j,i,t,dist,plane;
        for (int unit=1; unit<segment.size()-1; unit++) //going to omit first and last unit per segment
        {
            V_NeuronSWC_unit P1,P2,P0;
            P0 = segment.at(unit);
            P1 = segment.at(unit-1);
            P2 = segment.at(unit+1);
            double norm[] = {P2.x-P1.x,P2.y-P1.y,P2.z-P1.z};
            double rad=0;
            int pVal = pixelVal(data1d,dimNum,P0.x,P0.y,P0.z,c);
            if (pVal<50) { v3d_msg("pVal low, bad neuron, skipping"); continue; }
            int pValCircTot=0, runs=0;
            double check=0;
            do
            {
                rad += 0.2;
                //defining limits
                V3DLONG xLow = P0.x-rad; if(xLow<0) xLow=0;
                V3DLONG xHigh = P0.x+rad; if(xHigh>N-1) xHigh=N-1;
                V3DLONG yLow = P0.y-rad; if(yLow<0) yLow=0;
                V3DLONG yHigh = P0.y+rad; if(yHigh>M-1) yHigh=M-1;
                V3DLONG zLow = P0.z-rad; if(zLow<0) zLow=0;
                V3DLONG zHigh = P0.z+rad; if(zHigh>P-1) zHigh=P-1;

                //scanning through the pixels
                for (k = zLow; k <= zHigh; k+=0.2)
                {
                    for (j = yLow; j <= yHigh; j+=0.2)
                    {
                        for (i = xLow; i <= xHigh; i+=0.2)
                        {
                            t = (i-P0.x)*(i-P0.x)+(j-P0.y)*(j-P0.y)+(k-P0.z)*(k-P0.z);
                            dist = sqrt(t);
                            plane = (norm[0]*i+norm[1]*j+norm[2]*k-(norm[0]*P0.x+norm[1]*P0.y+norm[2]*P0.z));
                            //v3d_msg(QString("Dist %1, plane eq %2").arg(dist).arg(plane));
                            if (dist<=rad+0.2 && dist>=rad-0.2 && plane<=0.2 && plane>=-0.2)
                            {
                                int pValCirc = pixelVal(data1d,dimNum,i,j,k,c);
                                pValCircTot += pValCirc;
                                runs++;
                            }
                        }
                    }
                }
                //v3d_msg(QString("total pVal %1 in %2 runs, rad %3").arg(pValCircTot).arg(runs).arg(rad));
                if (runs==0) {check=255;}
                else {check=pValCircTot/runs;}
            } while (check > pVal*2/3);
            radTot += rad;
        }
        radAve = radTot/(segment.size()-2);
    }*/


    if (segment.size()>2)
    {
        double rad,radTot=0;
        for (int unit=1; unit<segment.size()-1; unit++) //going to omit first and last unit per segment
        {
            V_NeuronSWC_unit P1,P2,P0;
            P0 = segment.at(unit);
            P1 = segment.at(unit-1);
            P2 = segment.at(unit+1);
            double Vnum[] = {P2.x-P1.x,P2.y-P1.y,P2.z-P1.z};
            double Vnorm = sqrt(Vnum[0]*Vnum[0]+Vnum[1]*Vnum[1]+Vnum[2]*Vnum[2]);
            double V[] = {Vnum[0]/Vnorm,Vnum[1]/Vnorm,Vnum[2]/Vnorm}; //axis of rotation
            double Anum[] = {-V[1],V[0],(-V[1]*V[0]-V[0]*V[1])/V[2]};
            double Anorm = sqrt(Anum[0]*Anum[0]+Anum[1]*Anum[1]+Anum[2]*Anum[2]);
            double A[] = {Anum[0]/Anorm,Anum[1]/Anorm,Anum[2]/Anorm}; //perpendicular to V
            double B[] = {A[1]*V[2]-A[2]*V[1],A[2]*V[0]-A[0]*V[2],A[0]*V[1]-A[1]*V[0]}; //perpendicular to A and V
            rad=0;
            int pVal = pixelVal(data1d,dimNum,P0.x,P0.y,P0.z,c);
            if (pVal<50) { /*v3d_msg("pVal low, bad neuron, skipping");*/ continue; }
            int pValCircTot=0, runs=0;
            double check=255, pi=3.14;
            double x,y,z;
            do
            {
                rad += 0.2;
                for (double theta=0; theta<2*pi; theta+=pi/8)
                {
                    x = P0.x+rad*cos(theta)*A[0]+rad*sin(theta)*B[0];
                    y = P0.y+rad*cos(theta)*A[1]+rad*sin(theta)*B[1];
                    z = P0.z+rad*cos(theta)*A[2]+rad*sin(theta)*B[2];
                    int pValCirc = pixelVal(data1d,dimNum,x,y,z,c);
                    pValCircTot += pValCirc;
                    runs++;
                }
                check=pValCircTot/runs;
            } while (check > pVal*2/3);
            radTot += rad;
        }
        radAve = radTot/(segment.size()-2);
    }

    else //2-unit segment
    {
        v3d_msg("2-unit neuron SWC radius calculation to be implemented, please remove from test data");
    }

    return radAve;
}

//obsolete function
void markers_singleChannel(V3DPluginCallback2 &callback, QWidget *parent)
{
    v3dhandle curwin = callback.currentImageWindow();

    //cancels if no image
    if (!curwin)
    {
        v3d_msg("You don't have any image open in the main window.");
        return;
    }

    //if image, pulls the data
    Image4DSimple* p4DImage = callback.getImage(curwin); //the data of the image is in 4D (channel + 3D)

    unsigned char* data1d = p4DImage->getRawData(); //sets data into 1D array

    //defining the dimensions
    V3DLONG N = p4DImage->getXDim();
    V3DLONG M = p4DImage->getYDim();
    V3DLONG P = p4DImage->getZDim();
    V3DLONG sc = p4DImage->getCDim();
    //input channel
    unsigned int c=1, rad=10;
    bool ok;
    if (sc==1)
        c=1; //if only using 1 channel
    else
        c = QInputDialog::getInteger(parent, "Channel", "Enter Channel Number", 1, 1, sc, 1,&ok);

    //storing the dimensions
    V3DLONG dimNum[4];
    dimNum[0]=N; dimNum[1]=M; dimNum[2]=P; dimNum[3]=sc;

    //pulling marker info
    int xc,yc,zc;
    LocationSimple tmpLocation(0,0,0);
    LandmarkList mlist = callback.getLandmark(curwin);
    QString imgname = callback.getImageName(curwin);
    int marknum = mlist.count();
    if (mlist.isEmpty())
    {
        v3d_msg(QString("The marker list of the current image [%1] is empty. Do nothing.").arg(imgname));
        return;
    }
    else
    {
        //radius input
        rad = QInputDialog::getInteger(parent, "Radius", "Enter radius", 1,1,P,1,&ok);
        int cat = mlist.at(0).category;

        //dynamic array to store average pixel values of all markers
        int * markAve; int * pointAve;
        markAve = new int[marknum]; pointAve = new int[marknum];
        int markSum = 0, pointSum = 0, dataAve, pointval;

        //getting pixel values from each marker
        for (int i=0; i<marknum; i++)
        {
            tmpLocation = mlist.at(i);
            tmpLocation.getCoord(xc,yc,zc);
            pair<int,int> pixAns = pixel(data1d,dimNum,xc,yc,zc,c,rad);
            dataAve = pixAns.first;
            pointval = pixAns.second;
            markAve[i] = dataAve;
            pointAve[i] = pointval;
            markSum += dataAve;
            pointSum += pointval;
        }

        int MarkAve = markSum/marknum; //average pixel value of all markers in this channel
        int PointAve = pointSum/marknum;
        //st dev of markAve
        int stM=0, stP=0;
        for (int i=0; i<marknum; i++)
        {
            int s = pow(markAve[i]-MarkAve,2.0);
            stM += s;

            int t = pow(pointAve[i]-PointAve,2.0);
            stP += t;
        }
        int MarkStDev = sqrt(stM/(marknum-1.0));
        int PointStDev = sqrt(stP/(marknum-1.0));

        //v3d_msg(QString("Mean value region: %1. St Dev: %2 MarkSum %3 and markNum %4. Mean value point: %5. St Dev: %6").arg(MarkAve).arg(MarkStDev).arg(markSum).arg(marknum).arg(PointAve).arg(PointStDev));

        //and here we need to add the function that will scan the rest of the image for other cells based on MarkAve and MarkStDev
        //int CellCnt = count(data1d,dimNum,curwin,MarkAve,MarkStDev,rad,c);
        //v3d_msg(QString("There are %1 cells in this channel").arg(CellCnt));

        LandmarkList newList = count(data1d,dimNum,MarkAve,MarkStDev,PointAve,PointStDev,rad,0,0,c,cat);

        //now need to delete duplicate markers on same cell
        LandmarkList smallList = duplicates(data1d,newList,dimNum,PointAve,rad,c);
        LandmarkList& woot = smallList;
        bool draw_le_markers = callback.setLandmark(curwin,woot);

    }
    return;
}

bool export_list2file(QList<NeuronTree> & N2, QString fileSaveName, QString fileOpenName)
{
    QFile file(fileSaveName);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Text))
        return false;
    QTextStream myfile(&file);
    myfile<<"# generated by Vaa3D Plugin resample_swc"<<endl;
    myfile<<"# source file(s): "<<fileOpenName<<endl;
    myfile<<"# id,type,x,y,z,r,pid"<<endl;
    QList<NeuronSWC> N1;
    for (int j=0; j<N2.count(); j++)
    {
        N1 = N2.at(j).listNeuron;
        for (V3DLONG i=0;i<N1.size();i++)
        {
            myfile << N1.at(i).n <<" " << N1.at(i).type << " "<< N1.at(i).x <<" "<<N1.at(i).y << " "<< N1.at(i).z << " "<< N1.at(i).r << " " <<N1.at(i).pn << "\n";
        }
    }
    file.close();
    v3d_msg(QString("SWC file %1 has been generated, size %2").arg(fileSaveName).arg(N1.size()));
    cout<<"swc file "<<fileSaveName.toStdString()<<" has been generated, size: "<<N1.size()<<endl;
    return true;
};