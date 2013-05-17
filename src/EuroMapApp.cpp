// Map of Europe copyright Wikiepedia:
// http://en.wikipedia.org/wiki/File:Blank_map_of_Europe_-_Atelier_graphique_colors.svg

#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/svg/Svg.h"
#include "cinder/cairo/Cairo.h"
#include "cinder/Timeline.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Fbo.h"
#include "cinder/TriMesh.h"
#include "cinder/triangulate.h"
#include "cinder/gl/Vbo.h"

#include <iostream>
//#include <set>
#include <algorithm>
#include <vector>

using namespace ci;
using namespace ci::app;
using namespace std;

class EuroMapApp : public AppBasic {
  public:
  	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );
	void mouseUp( MouseEvent event );
	void mouseMove( MouseEvent event );
	void mouseWheel( MouseEvent event );
	void update();
	void draw();
	void reparametrize();
	Vec2f getLinearPosition( float t );
	gl::Texture renderSvgToTexture2(svg::DocRef doc, Vec2i size);
	gl::Texture renderSvgGroupToTexture2( const svg::Doc &doc, const string &groupName, Vec2i size, bool alpha );
	Vec2i GetPointAfterScale(Vec2i point);
	void resizeFBO(Vec2i size);
	void resizeFBO2(Vec2i size);
	void DrawShapeShader();
	void UpdateMaskCurrentCountry();
	void recalcShapeMeshes(Shape2d *shape);

	gl::Texture			mMapTex;
	gl::Texture			mRussieTex;
	gl::Texture			mInfoTexture;
	//gl::Texture			mRussieAlpha;
	gl::TextureFontRef	mFont;
	svg::DocRef			mMapDoc;
	svg::Node 			*mCurrentCountry;
	svg::Node			*mCurrentCountryShader;
	svg::Node			*mCurrentPath;
	Anim<float>			mCurrentCountryAlpha;
	Anim<float>			mCurrentPathAlpha;
	Anim<float>			mScale;
	Anim<float>			mPathPos;
	Path2d				mPath;
	int					RESOLUTION;
	vector<float>		m_parametrization;
	float               m_totalLength;

	TimelineRef			mTimelineScale;
	TimelineRef			mTimelinePath;
	double				mScaleCtx;
	float				mScaleMouse;

	gl::TextureRef	mTexture;
	gl::GlslProgRef	mShader;
	float			mAngle;

	gl::Fbo				mFbo;
	gl::VboMesh			mShapeVboMesh;
};

gl::Texture renderSvgToTexture( svg::DocRef doc, Vec2i size )
{
	cairo::SurfaceImage srf( size.x, size.y, false );
	cairo::Context ctx( srf );
	ctx.render( *doc );
	srf.flush();
	return gl::Texture( srf.getSurface() );
}
gl::Texture EuroMapApp::renderSvgToTexture2(svg::DocRef doc, Vec2i size) {
    
	
    if ((double)size.x / doc->getWidth()  < (double)size.y / doc->getHeight()) {
        mScaleCtx = (double)size.x / doc->getWidth();
    } else {
        mScaleCtx = (double)size.y / doc->getHeight();
    }

    cairo::SurfaceImage srf(ceil((double)(doc->getWidth()) * mScaleCtx), ceil((double)(doc->getHeight()) * mScaleCtx), true);
    cairo::Context ctx(srf);
    ctx.scale(mScaleCtx, mScaleCtx);
    ctx.render(*doc);
    srf.flush();
    return gl::Texture(srf.getSurface());
}
// Renders a given SVG group 'groupName' into a new gl::Texture
gl::Texture renderSvgGroupToTexture( const svg::Doc &doc, const std::string &groupName, const Rectf &rect, bool alpha )
{
	cairo::SurfaceImage srfImg( rect.getWidth(), rect.getHeight(), alpha );
	cairo::Context ctx( srfImg );
	ctx.scale( rect.getWidth() / doc.getWidth(), rect.getHeight() / doc.getHeight() );
	ctx.render( doc / groupName );
	return gl::Texture( srfImg.getSurface() );
}

// Renders a given SVG group 'groupName' into a new gl::Texture
gl::Texture EuroMapApp::renderSvgGroupToTexture2( const svg::Doc &doc, const std::string &groupName, Vec2i size, bool alpha )
{
	if ((double)size.x / doc.getWidth()  < (double)size.y / doc.getHeight()) {
        mScaleCtx = (double)size.x / doc.getWidth();
    } else {
        mScaleCtx = (double)size.y / doc.getHeight();
    }
	cairo::SurfaceImage srf(ceil((double)(doc.getWidth()) * mScaleCtx), ceil((double)(doc.getHeight()) * mScaleCtx), alpha);
    cairo::Context ctx(srf);
    ctx.scale(mScaleCtx, mScaleCtx);
    ctx.render( doc / groupName );
    srf.flush();
    return gl::Texture(srf.getSurface());
}
void EuroMapApp::prepareSettings( Settings *settings )
{
	settings->setResizable( false );
	settings->setWindowSize( 1920, 1080 );
}
void EuroMapApp::resizeFBO(Vec2i size)
{
	// create or resize framebuffer if needed
	if(!mFbo || mFbo.getWidth() != size.x || mFbo.getHeight() != size.y) {
		gl::Fbo::Format fmt;


		// we create multiple color targets:
		//  -one for the scene as we will view it
		//  -one to contain a color coded version of the scene that we can use for picking
		//fmt.enableColorBuffer( true, 2 );

		// enable multi-sampling for better quality 
		//  (if this sample does not work on your computer, try lowering the number of samples to 2 or 0)
		//fmt.setSamples(4);

		// create the buffer
		mFbo = gl::Fbo( size.x, size.y, fmt );
	}
}

void EuroMapApp::resizeFBO2(Vec2i size)
{
	int newsize;
	if (size.x > size.y){ newsize = size.x;}
	else {newsize = size.y;}
	// create or resize framebuffer if needed
	if(!mFbo || mFbo.getWidth() != newsize || mFbo.getHeight() != newsize) {
		gl::Fbo::Format fmt;

		// create the buffer
		mFbo = gl::Fbo( newsize, newsize, fmt );
	}
}


void EuroMapApp::recalcShapeMeshes(Shape2d *shape)
{
 TriMesh2d mesh = Triangulator( *shape).calcMesh( Triangulator::WINDING_ODD );
 mShapeVboMesh = gl::VboMesh( mesh );
}

void EuroMapApp::setup()
{
	//gl::enableDepthRead();
	//gl::enableDepthWrite();	
	mScaleCtx = 1.0;
	mMapDoc = svg::Doc::create( loadAsset( "Europe4.svg" ) );

	// DISPLAY NONE
	svg::Style style;
	//style.setVisible(false);
	style.setDisplayNone(true);
	//(const_cast<svg::Node*>(mMapDoc->findNodeByIdContains("France")))->setStyle(style);
	svg::Node *node = (const_cast<svg::Node*>(mMapDoc->findNodeByIdContains("Chemin")));
	if (node) node->setStyle(style);
	node = (const_cast<svg::Node*>(mMapDoc->findNodeByIdContains("RussieInfos")));
	if (node) node->setStyle(style);

	mMapTex = renderSvgToTexture2( mMapDoc, getWindowSize()*getWindowContentScale() );
	mRussieTex = renderSvgGroupToTexture2( *mMapDoc,"RussieInfos",  getWindowSize()*getWindowContentScale(), true );
	//mRussieAlpha = renderSvgGroupToTexture2( *mMapDoc,"Russian-Federation-mask",  getWindowSize()*getWindowContentScale(), true );
	//mMapTex = renderSvgToTexture( mMapDoc, getWindowSize());	
	mFont = gl::TextureFont::create( Font( loadAsset( "Dosis-Medium.ttf" ), 36 ) );
	
	mCurrentCountry = 0;
	mCurrentPath = NULL;
	timeline().setLoop( true );
	mTimelineScale = Timeline::create();
	//mTimelineScale->setStartTime(0);
	mTimelineScale->setDefaultAutoRemove( false );
	mTimelineScale->setLoop( true );
	mTimelineScale->apply( &mScale, 0.5f, 2.0f, 2.0f );
	mTimelineScale->appendTo( &mScale, 2.0f, 0.5f, 2.0f );
	timeline().add( mTimelineScale );
	
	mTimelinePath = Timeline::create();
	//mTimelineScale->setStartTime(0);
	mTimelinePath->setDefaultAutoRemove( false );
	mTimelinePath->setDefaultAutoRemove( false );
	mTimelinePath->setLoop( true );
	mTimelinePath->apply( &mPathPos, 0.0f,1.f, 2.0f );
	mTimelinePath->setStartTime(getElapsedSeconds());
    timeline().add( mTimelinePath );
	cout<<"mScale.value() = "<<mScale.value()<<endl;
	mScaleMouse = 1.0f;

	RESOLUTION = 100;
	m_totalLength = 0.0f;

	try {
		mTexture = gl::Texture::create( loadImage(  loadAsset("cinder_logo2.png" ) ) );
	}
	catch( ... ) {
		std::cout << "unable to load the texture file!" << std::endl;
	}
	try {
		mShader = gl::GlslProg::create(  loadAsset( "passThru_vert.glsl" ), loadAsset( "gaussianBlur_frag.glsl" ) );
	}
	catch( gl::GlslProgCompileExc &exc ) {
		std::cout << "Shader compile error: " << std::endl;
		std::cout << exc.what();
	}
	catch( ... ) {
		std::cout << "Unable to load shader" << std::endl;
	}
	
	mAngle = 0.0f;
	gl::Fbo::Format format;
	// même taille que la texture de la map SVG
	mFbo = gl::Fbo( ceil((double)(mMapDoc->getWidth()) * mScaleCtx),ceil((double)(mMapDoc->getHeight()) * mScaleCtx), format );

	// initialize mShapeVboMesh
	svg::Node * nodeFrance = const_cast<svg::Node*>(mMapDoc->findNodeByIdContains("France"));
	Shape2d pathShape = nodeFrance->getShape();
	recalcShapeMeshes(&pathShape);

	mCurrentCountryShader = NULL;

}

void EuroMapApp::reparametrize() 
{			
	float resolution = (float) RESOLUTION;
			
	// find approximate length of curve
	m_parametrization.clear();
			
	m_totalLength = 0.0f;
	for(int i=0;i<RESOLUTION;++i) {
		//m_parametrization.insert( m_totalLength );
		m_parametrization.push_back( m_totalLength );
				
		if( (i+1) < RESOLUTION ) {
		Vec2f p0 = mPath.getPosition( (i+0) / resolution );
		Vec2f p1 = mPath.getPosition( (i+1) / resolution );
		m_totalLength += p0.distance(p1);
		}
	}
		
	for(int i=0;i<RESOLUTION;++i) 
		m_parametrization[i] /= m_totalLength;

	sort(m_parametrization.begin(), m_parametrization.end());	
}

Vec2f EuroMapApp::getLinearPosition( float t ) 
{
	float resolution = (float) (RESOLUTION-1);

	// perform binary search to find the elements 
	// closest to the desired distance
	//set<float>::iterator itr = m_parametrization.lower_bound( t );
	vector<float>::iterator itr = upper_bound(m_parametrization.begin(), m_parametrization.end(), t );
	//vector<float>::iterator low = lower_bound (m_parametrization.begin(), m_parametrization.end(), t ); // 
	// no value found 
	if( itr == m_parametrization.end() )
	return mPath.getPosition( 1.0f );
	else if( itr == m_parametrization.begin() )
		return mPath.getPosition( 0.0f );

	// exact value found
	if( *itr == t ) {
		float n = ( itr - m_parametrization.begin() ) / resolution;
		return mPath.getPosition( n );
	}

	// approximate value of 't' by interpolation
	int upper = itr - m_parametrization.begin(); // index of upper bound
	int lower = itr -1 -  m_parametrization.begin(); // index of lower bound
	//console()<<"upper = "<<upper<<"   "<<endl;
	//console()<<"lower = "<<lower<<"   "<<endl;
	//console()<<"m_parametrization = ";
	//for (int i = 0 ; i < m_parametrization.size() ; i++)
	//{
	//	console()<<m_parametrization.at(i)<<", ";
	//}
	//console()<<endl;
	float n = (t - m_parametrization[lower]) / (m_parametrization[upper] - m_parametrization[lower]);
	//console()<<"n = "<<n<<"   "<<endl;
	n = lerp( lower / resolution, upper / resolution, n );
	//console()<<"n lerp= "<<n<<"   get = "<<mPath.getPosition( n )<<endl;
	// return position on path
	return mPath.getPosition( n );
}

void EuroMapApp::mouseDown( MouseEvent event )
{
	if( event.isLeftDown() )
	{
		svg::Node *newNode = mMapDoc->nodeUnderPoint(GetPointAfterScale(event.getPos()));
		if( newNode->getId() == "Poland" )
		{
			mCurrentPath = const_cast<svg::Node*>(mMapDoc->findNodeByIdContains("FrancePologne"));
			Shape2d pathShape = mCurrentPath->getShape();
			mPath = pathShape.getContour( 0 );
			reparametrize();
			//mTimelinePath->apply( &mPathPos, 0.0f,1.f, 2.0f );
			mTimelinePath->setStartTime(getElapsedSeconds());
			timeline().apply( &mCurrentPathAlpha, 0.0f, 1.0f, 1.0f );
		}
		else if( newNode->getParent()->getId() == "Russian-Federation" )
		{
			mCurrentPath = const_cast<svg::Node*>(mMapDoc->findNodeByIdContains("FranceRussie"));
			Shape2d pathShape = mCurrentPath->getShape();
			mPath = pathShape.getContour( 0 );
			reparametrize();
			//mTimelinePath->apply( &mPathPos, 0.0f,1.f, 2.0f );
			mTimelinePath->setStartTime(getElapsedSeconds());
			timeline().apply( &mCurrentPathAlpha, 0.0f, 1.0f, 1.0f );
		}
		else mCurrentPath = NULL;
		
		// if the current node has no name just set it to NULL
		if( mCurrentPath && mCurrentPath->getId().empty())
			mCurrentPath = NULL;
		UpdateMaskCurrentCountry();
		//if (mCurrentCountry) recalcShapeMeshes(&mCurrentCountry->getShapeAbsolute());
		
	}
}
void EuroMapApp::mouseWheel( MouseEvent event )
{
	mScaleMouse += event.getWheelIncrement()/100.f;
	if (mScaleMouse < 0.f) mScaleMouse = 0.01f;
}
void EuroMapApp::mouseUp( MouseEvent event )
{
	//mCurrentPath = NULL;
}
Vec2i EuroMapApp::GetPointAfterScale(Vec2i point)
{
	
	//svg::Node *newNode = mMapDoc->nodeUnderPoint( Vec2i(Vec2f(event.getPos())/ (mScale.value()*(float)(mScaleCtx))) );
	//Vec2i point = Vec2i(event.getPos().x,event.getPos().y);/// ((float)(mScaleCtx)));
	point += Vec2i (-(getWindowWidth()-mMapTex.getWidth ()*mScaleMouse)/2.f, -(getWindowHeight()-mMapTex.getHeight ()*mScaleMouse)/2.f);
	point = Vec2i(Vec2f(point)/ (float)(mScaleCtx));
	//point += Vec2i (getWindowWidth()/2,getWindowHeight()/2);
	point = Vec2i(Vec2f(point)/ (mScaleMouse));
	//point += Vec2i (-mMapTex.getWidth ()/2.0f, -mMapTex.getHeight ()/2.0f);
	
	// pour éviter une erreur lorsque on est en dehors de la zone svg il faut eviter de déborder
	if (point.x >= mMapDoc->getWidth()-1) point.x = mMapDoc->getWidth()-1 ;
	if (point.x <= 1) point.x = 1 ;
	if (point.y >= mMapDoc->getHeight()-1) point.y = mMapDoc->getHeight()-1 ;
	if (point.y <= 1) point.y = 1 ;
	return point;
}
void EuroMapApp::mouseMove( MouseEvent event )
{
	//mScaleMouse = (float) (event.getPos().y) /(float) (getWindowHeight())+0.5f;

	svg::Node *newNode = mMapDoc->nodeUnderPoint(GetPointAfterScale(event.getPos()));
	if( newNode != mCurrentCountry )
	{
		timeline().apply( &mCurrentCountryAlpha, 0.0f, 1.0f, 1.0f );
		
		/*
		svg::Style style;
		style.setVisible(true);
		style.setFill(svg::Paint(Color(1.0f,0.0f,0.0f)));
		newNode->setStyle(style);
		mMapTex = renderSvgToTexture2( mMapDoc, getWindowSize()*getWindowContentScale() );
		*/
		
	}
	mCurrentCountry = newNode;
	// if the current node has no name just set it to NULL
	if( mCurrentCountry && mCurrentCountry->getId().empty()||(mCurrentCountry->getParent()->getId() == "Chemin") )
		mCurrentCountry = NULL;	
}
void EuroMapApp::update()
{
		TextLayout infoText;
		infoText.clear( ColorA( 0.2f, 0.2f, 0.2f, 0.5f ) );
		infoText.setColor( Color::white() );
		infoText.addCenteredLine( toString( this->getAverageFps() ) + " fps" );
		infoText.setBorder( 4, 2 );
		mInfoTexture = gl::Texture( infoText.render( true ) );
}
void EuroMapApp::UpdateMaskCurrentCountry()
{
	if( mCurrentCountry ) {
		mCurrentCountryShader = mCurrentCountry;
		resizeFBO2(Vec2i(mCurrentCountry->getBoundingBoxAbsolute().getWidth(), mCurrentCountry->getBoundingBoxAbsolute().getHeight())); //Absolute()
		//console()<<"mFbo.getHeight="<<mFbo.getHeight()<<", mFbo.getWidth()="<<mFbo.getWidth()<<endl;
		mFbo.bindFramebuffer();
		gl::pushMatrices();
		gl::enableAlphaBlending();
		// flip verticaly
		gl::pushModelView();
		gl::translate( Vec2f( 0.0f, ceil((double)(mMapDoc->getHeight()) * mScaleCtx) ) );//getWindowHeight()
		//gl::translate( Vec2f( 0.0f, mFbo.getHeight() ) );
		gl::scale( Vec2f(1, -1) );
		gl::translate(-mCurrentCountry->getBoundingBoxAbsolute().x1, -mCurrentCountry->getBoundingBoxAbsolute().y1);
		gl::clear( ColorA( 0.0f, 0.0f, 0.0f,0.0f ) );
		gl::color( 0.0f, 1.0f, 0.0f,1.0f );
		gl::drawSolid( mCurrentCountry->getShapeAbsolute(),2.0);//Absolute
		//gl::drawSolidRect(mCurrentCountry->getBoundingBox());

		// restore the flip
		gl::popModelView();

		//gl::disableAlphaBlending();
		gl::popMatrices();
		mFbo.unbindFramebuffer();
	}
}
void EuroMapApp::DrawShapeShader()
{
		if (mCurrentCountryShader)
		{
		//FBO method
		gl::pushMatrices();
		gl::translate(mCurrentCountryShader->getBoundingBoxAbsolute().x1, mCurrentCountryShader->getBoundingBoxAbsolute().y1);
		
		gl::enableAlphaBlending();
		//glEnable( GL_TEXTURE_2D );
		mTexture->bind(0);
		mFbo.getTexture().bind(1);
		mShader->bind();
		mShader->uniform( "tex0", 0 );
		mShader->uniform("texmask",1);
		mShader->uniform( "sampleOffset", Vec2f( cos( mAngle ), sin( mAngle ) ) * ( 3.0f / getWindowWidth() ) );
		//Rectf rectDoc = Rectf(0,0,ceil((double)(mMapDoc->getWidth()) * mScaleCtx),ceil((double)(mMapDoc->getHeight()) * mScaleCtx));
		gl::drawSolidRect( mFbo.getBounds());
		mShader->unbind();
		mFbo.getTexture().unbind(1);
		mTexture->unbind(0);

		gl::popMatrices();
		
		//VBO method
	/*
		gl::enableAlphaBlending();
		//glEnable( GL_TEXTURE_2D );
		mTexture->bind(0);
		mShader->bind();
		mShader->uniform( "tex0", 0 );
		mShader->uniform( "sampleOffset", Vec2f( cos( mAngle ), sin( mAngle ) ) * ( 3.0f / getWindowWidth() ) );
		gl::draw(mShapeVboMesh);
		mShader->unbind();
		mTexture->unbind(0);
		*/
		}
}
void EuroMapApp::draw()
{
	//update
	mAngle += 0.05f;


	gl::clear( Color( 0, 0, 0 ) );
	gl::enableAlphaBlending();
	glLineWidth( 2.0f );
	//y = abs((x++ % 6) - 3);
	//mMapTex = renderSvgToTextureScale(mMapDoc, getWindowSize()* getWindowContentScale() , mScale.value());//
	//DrawShapeShader();
	gl::pushMatrices();
	//gl::scale( mScale.value(), mScale.value(),1.f);
	gl::translate( getWindowWidth()/2.0f, getWindowHeight()/2.0f, 0.0f ); // scale to the center
	gl::scale(mScaleMouse,mScaleMouse,1.0f);
	gl::translate( -mMapTex.getWidth ()/2.0f, -mMapTex.getHeight ()/2.0f, 0.0f );
	if( mMapTex ) {
		gl::color( Color::white() );
		gl::draw( mMapTex );//,Vec2f( -mMapTex.getWidth ()/2.0f, -mMapTex.getHeight ()/2.0f)	
	}
	

	if( mCurrentCountry ) {
		gl::pushMatrices();
		//gl::translate( getWindowWidth()/2.0f, getWindowHeight()/2.0f, 0.0f );
		gl::scale( mScaleCtx, mScaleCtx,1.f);
		// draw the outline
		gl::color( 1, 0.5f, 0.25f, mCurrentCountryAlpha );
		//mCurrentCountry->getShapeAbsolute().scale(Vec2f((float)(1/mScaleCtx),(float)(1/mScaleCtx)));
		gl::draw( mCurrentCountry->getShapeAbsolute(),2.0);
		gl::color( 0.7f, 0.7f, 0.4f, mCurrentCountryAlpha );
		gl::drawSolid( mCurrentCountry->getShapeAbsolute(),2.0);
		DrawShapeShader();
		// draw the name
		string countryName = mCurrentCountry->getId();
		Vec2f pos = mCurrentCountry->getBoundingBoxAbsolute().getCenter();
		pos.x -= mFont->measureString( countryName ).x / 2;
		
		gl::color( ColorA( 1, 1, 1, mCurrentCountryAlpha ) );
		mFont->drawString( countryName, pos + Vec2f( 2, 2 ) );
		gl::color( ColorA( 0, 0, 0, mCurrentCountryAlpha ) );
		mFont->drawString( countryName, pos );
		gl::popMatrices();
	}
	if ((mCurrentCountry )&&(mCurrentCountry->getParent()->getId() == "Russian-Federation") && mRussieTex) 
	{ 
		gl::color( Color::white() );
		gl::draw( mRussieTex );
	}
	if (mCurrentPath) {
		gl::pushMatrices();
		//gl::translate( getWindowWidth()/2.0f, getWindowHeight()/2.0f, 0.0f );
		gl::scale( mScaleCtx, mScaleCtx,1.f);
		// draw the outline
		gl::color( 1, 0.5f, 0.25f, mCurrentPathAlpha );
		gl::draw( mCurrentPath->getShapeAbsolute(),2.0);
		Vec2f objectPos = getLinearPosition(mPathPos.value());
		//Vec2f objectPos = mPath.getPosition( mPathPos.value());
		gl::color( 1.0f, 0.0f, 0.0f );
		gl::drawSolidCircle( objectPos, 15.0f );	
		// draw the name
		/*
		string pathName = mCurrentPath->getId();
		Vec2f pos = mCurrentPath->getBoundingBoxAbsolute().getCenter();
		pos.x -= mFont->measureString( pathName ).x / 2;
		
		gl::color( ColorA( 1, 1, 1, mCurrentPathAlpha ) );
		mFont->drawString( pathName, pos + Vec2f( 2, 2 ) );
		gl::color( ColorA( 0, 0, 0, mCurrentPathAlpha ) );
		mFont->drawString( pathName, pos );
		*/
		gl::popMatrices();
	}
	gl::popMatrices();
	
	if( mInfoTexture ) {
			gl::color(Color::white());
			//glDisable( GL_TEXTURE_RECTANGLE_ARB );
			gl::draw( mInfoTexture, Vec2f( 20, getWindowHeight() - 20 - mInfoTexture.getHeight() ) );
			//gl::draw( mInfoTexture, Vec2f( mInfoTexture.getWidth()/2.0f, mInfoTexture.getHeight()/2.0f) );
		};
}


CINDER_APP_BASIC( EuroMapApp, RendererGl(0) )
