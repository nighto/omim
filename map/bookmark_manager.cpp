#include "bookmark_manager.hpp"
#include "framework.hpp"

#include "../platform/platform.hpp"
#include "../platform/settings.hpp"

#include "../indexer/scales.hpp"

#include "../base/stl_add.hpp"

#include "../std/algorithm.hpp"
#include "../std/target_os.hpp"
#include "../std/vector.hpp"


BookmarkManager::BookmarkManager(Framework & f)
  : m_framework(f), m_bmScreen(0)
{
  m_additionalPoiLayer = new BookmarkCategory("");
}

BookmarkManager::~BookmarkManager()
{
  delete m_additionalPoiLayer;
  ClearItems();
  DeleteScreen();
}

namespace
{
char const * BOOKMARK_CATEGORY = "LastBookmarkCategory";
char const * BOOKMARK_TYPE = "LastBookmarkType";
}

void BookmarkManager::SaveState() const
{
  Settings::Set(BOOKMARK_CATEGORY, m_lastCategoryUrl);
  Settings::Set(BOOKMARK_TYPE, m_lastType);
}

void BookmarkManager::LoadState()
{
  Settings::Get(BOOKMARK_CATEGORY, m_lastCategoryUrl);
  Settings::Get(BOOKMARK_TYPE, m_lastType);
}

void BookmarkManager::DrawCategory(BookmarkCategory const * cat, shared_ptr<PaintEvent> const & e)
{
  Navigator & navigator = m_framework.GetNavigator();
  InformationDisplay & informationDisplay  = m_framework.GetInformationDisplay();

  // get viewport limit rect
  m2::AnyRectD const & glbRect = navigator.Screen().GlobalRect();
  Drawer * pDrawer = e->drawer();

 // Draw tracks. Only track with display list will be drawn
  for (size_t i = 0; i < cat->GetTracksCount(); ++i)
    cat->GetTrack(i)->Draw(e);

  // Draw bookmarks
  for (size_t j = 0; j < cat->GetBookmarksCount(); ++j)
  {
    Bookmark const * bm = cat->GetBookmark(j);
    m2::PointD const & org = bm->GetOrg();

    // draw only visible bookmarks on screen
    if (glbRect.IsPointInside(org))
      informationDisplay.drawPlacemark(pDrawer, bm->GetType().c_str(), navigator.GtoP(org));
  }

}

void BookmarkManager::ClearItems()
{
  for_each(m_categories.begin(), m_categories.end(), DeleteFunctor());
  m_categories.clear();
}

void BookmarkManager::LoadBookmarks()
{
  ClearItems();

  string const dir = GetPlatform().WritableDir();

  Platform::FilesList files;
  Platform::GetFilesByExt(dir, BOOKMARKS_FILE_EXTENSION, files);
  for (size_t i = 0; i < files.size(); ++i)
    LoadBookmark(dir+files[i]);

  LoadState();
}

void BookmarkManager::LoadBookmark(string const & filePath)
{
  BookmarkCategory * cat = BookmarkCategory::CreateFromKMLFile(filePath);
  if (cat)
    m_categories.push_back(cat);
  }

size_t BookmarkManager::AddBookmark(size_t categoryIndex, Bookmark & bm)
{
  bm.SetTimeStamp(time(0));
  bm.SetScale(m_framework.GetDrawScale());

  BookmarkCategory * pCat = m_categories[categoryIndex];
  pCat->AddBookmark(bm);
  pCat->SetVisible(true);
  pCat->SaveToKMLFile();

  m_lastCategoryUrl = pCat->GetFileName();
  m_lastType = bm.GetType();
  SaveState();

  return (pCat->GetBookmarksCount() - 1);
}

void BookmarkManager::ReplaceBookmark(size_t catIndex, size_t bmIndex, Bookmark const & bm)
{
  BookmarkCategory * pCat = m_categories[catIndex];
  pCat->ReplaceBookmark(bmIndex, bm);
  pCat->SaveToKMLFile();

  m_lastType = bm.GetType();
  SaveState();
}

size_t BookmarkManager::LastEditedBMCategory()
{
  for (int i = 0; i < m_categories.size();++i)
  {
    if (m_categories[i]->GetFileName() == m_lastCategoryUrl)
      return i;
  }

  if (m_categories.empty())
    m_categories.push_back(new BookmarkCategory(m_framework.GetStringsBundle().GetString("my_places")));

  return 0;
  }

string BookmarkManager::LastEditedBMType() const
{
  return (m_lastType.empty() ? BookmarkCategory::GetDefaultType() : m_lastType);
}

BookmarkCategory * BookmarkManager::GetBmCategory(size_t index) const
{
  return (index < m_categories.size() ? m_categories[index] : 0);
}

size_t BookmarkManager::CreateBmCategory(string const & name)
{
  m_categories.push_back(new BookmarkCategory(name));
  return (m_categories.size()-1);
}

void BookmarkManager::Update(Navigator & navigator)
{
  for (int i = 0; i < m_categories.size(); ++i)
    for (int j = 0; j < m_categories[i]->GetTracksCount(); ++j)
      m_categories[i]->GetTrack(j)->UpdateDisplayList(navigator, m_bmScreen);
}

void BookmarkManager::DrawItems(shared_ptr<PaintEvent> const & e)
{
  e->drawer()->screen()->beginFrame();

  for (size_t i = 0; i < m_categories.size(); ++i)
  {
    if (GetBmCategory(i)->IsVisible())
      DrawCategory(m_categories[i], e);
  }
  if (m_additionalPoiLayer->IsVisible())
    DrawCategory(m_additionalPoiLayer, e);

  e->drawer()->screen()->endFrame();
}

void BookmarkManager::DeleteBmCategory(CategoryIter i)
{
  BookmarkCategory * cat = *i;

  FileWriter::DeleteFileX(cat->GetFileName());

  delete cat;

  m_categories.erase(i);
}

bool BookmarkManager::DeleteBmCategory(size_t index)
{
  if (index < m_categories.size())
  {
    DeleteBmCategory(m_categories.begin() + index);
    return true;
  }
  else
    return false;
}

void BookmarkManager::AdditionalPoiLayerSetInvisible()
{
  m_additionalPoiLayer->SetVisible(false);
}

void BookmarkManager::AdditionalPoiLayerSetVisible()
{
  m_additionalPoiLayer->SetVisible(true);
}

void BookmarkManager::AdditionalPoiLayerAddPoi(Bookmark const & bm)
{
  m_additionalPoiLayer->AddBookmark(bm);
}

Bookmark const * BookmarkManager::AdditionalPoiLayerGetBookmark(size_t index) const
{
  return m_additionalPoiLayer->GetBookmark(index);
}

Bookmark * BookmarkManager::AdditionalPoiLayerGetBookmark(size_t index)
{
  return m_additionalPoiLayer->GetBookmark(index);
}

void BookmarkManager::AdditionalPoiLayerDeleteBookmark(int index)
{
  m_additionalPoiLayer->DeleteBookmark(index);
}

void BookmarkManager::AdditionalPoiLayerClear()
{
  m_additionalPoiLayer->ClearBookmarks();
}

bool BookmarkManager::IsAdditionalLayerPoi(const BookmarkAndCategory & bm) const
{
  return (bm.first == additionalLayerCategory && bm.second >= 0);
}

void BookmarkManager::SetScreen(graphics::Screen * screen)
{
  DeleteScreen();
  m_bmScreen = screen;
}

void BookmarkManager::DeleteScreen()
{
  if (m_bmScreen)
  {
    // Delete display lists for all tracks
    for (size_t i = 0; i < m_categories.size(); ++i)
      for (size_t j = 0; j < m_categories[i]->GetTracksCount(); ++j)
        m_categories[i]->GetTrack(j)->DeleteDisplayList();


    delete m_bmScreen;
    m_bmScreen = 0;
  }
}
