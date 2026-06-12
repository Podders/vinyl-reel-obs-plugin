#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QSaveFile>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QResizeEvent>
#include <QTabWidget>
#include <QSize>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <curl/curl.h>

#include <functional>
#include <filesystem>
#include <thread>

OBS_DECLARE_MODULE()

namespace {
constexpr const char *kDockId = "vinyl_reel_dock";
constexpr const char *kSettingsOrg = "VinylReel";
constexpr const char *kSettingsApp = "VinylReelOBS";
constexpr const char *kArtworkSourceName = "Vinyl Reel Artwork";
constexpr const char *kDiscogsUserAgent = "VinylReelOBS/0.1";
constexpr const char *kDiscogsBaseUrl = "https://api.discogs.com";
constexpr int kImageMargin = 24;

struct DiscogsRelease {
	int collection_item_id = 0;
	int release_id = 0;
	QString title;
	QString artists;
	QString labels;
	int year = 0;
	QString artwork_url;
	QString cache_path;
};

QString joinNames(const QJsonArray &items, const QString &key)
{
	QStringList names;
	for (const auto &value : items) {
		const QJsonObject obj = value.toObject();
		const QString name = obj.value(key).toString().trimmed();
		if (!name.isEmpty()) {
			names.push_back(name);
		}
	}
	return names.join(", ");
}

QString cacheDirForArtworks()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	const QString path = QDir(base).filePath("artwork-cache");
	const QByteArray utf8_path = path.toUtf8();
	std::filesystem::create_directories(std::filesystem::u8path(utf8_path.constData()));
	return path;
}

QString buildReleaseLabel(const DiscogsRelease &release)
{
	QStringList parts;
	if (!release.artists.isEmpty()) {
		parts.push_back(release.artists);
	}
	if (!release.title.isEmpty()) {
		parts.push_back(release.title);
	}
	if (release.year > 0) {
		parts.push_back(QString::number(release.year));
	}
	return parts.join(" - ");
}

QString releaseCachePath(int release_id)
{
	const QString dir = cacheDirForArtworks();
	return QDir(dir).filePath(QString::number(release_id) + ".png");
}

QString collectionCacheDir()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	const QString path = QDir(base).filePath("collection-cache");
	const QByteArray utf8_path = path.toUtf8();
	std::filesystem::create_directories(std::filesystem::u8path(utf8_path.constData()));
	return path;
}

QString collectionCachePathForUsername(const QString &username)
{
	QString key = username.trimmed().toLower();
	key.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("_"));
	if (key.isEmpty()) {
		key = QStringLiteral("unknown");
	}

	return QDir(collectionCacheDir()).filePath(key + QStringLiteral(".json"));
}

QJsonObject releaseToJson(const DiscogsRelease &release)
{
	QJsonObject obj;
	obj.insert(QStringLiteral("collection_item_id"), release.collection_item_id);
	obj.insert(QStringLiteral("release_id"), release.release_id);
	obj.insert(QStringLiteral("title"), release.title);
	obj.insert(QStringLiteral("artists"), release.artists);
	obj.insert(QStringLiteral("labels"), release.labels);
	obj.insert(QStringLiteral("year"), release.year);
	obj.insert(QStringLiteral("artwork_url"), release.artwork_url);
	obj.insert(QStringLiteral("cache_path"), release.cache_path);
	return obj;
}

DiscogsRelease releaseFromJson(const QJsonObject &obj)
{
	DiscogsRelease release;
	release.collection_item_id = obj.value(QStringLiteral("collection_item_id")).toInt();
	release.release_id = obj.value(QStringLiteral("release_id")).toInt();
	release.title = obj.value(QStringLiteral("title")).toString();
	release.artists = obj.value(QStringLiteral("artists")).toString();
	release.labels = obj.value(QStringLiteral("labels")).toString();
	release.year = obj.value(QStringLiteral("year")).toInt();
	release.artwork_url = obj.value(QStringLiteral("artwork_url")).toString();
	release.cache_path = obj.value(QStringLiteral("cache_path")).toString();
	if (release.cache_path.isEmpty()) {
		release.cache_path = releaseCachePath(release.release_id);
	}
	return release;
}

bool saveCollectionCache(const QString &username, const QVector<DiscogsRelease> &releases, QString &error)
{
	const QString path = collectionCachePathForUsername(username);
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		error = QStringLiteral("Unable to open collection cache for writing.");
		return false;
	}

	QJsonArray release_array;
	for (const DiscogsRelease &release : releases) {
		release_array.append(releaseToJson(release));
	}

	QJsonObject root;
	root.insert(QStringLiteral("username"), username);
	root.insert(QStringLiteral("total_releases"), releases.size());
	root.insert(QStringLiteral("releases"), release_array);

	file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	if (!file.commit()) {
		error = QStringLiteral("Unable to commit collection cache.");
		return false;
	}

	return true;
}

bool loadCollectionCache(const QString &username, QVector<DiscogsRelease> &releases, int &total_releases, QString &error)
{
	releases.clear();
	total_releases = 0;

	const QString path = collectionCachePathForUsername(username);
	QFile file(path);
	if (!file.exists()) {
		return false;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		error = QStringLiteral("Unable to open collection cache.");
		return false;
	}

	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	const QJsonObject root = doc.object();
	total_releases = root.value(QStringLiteral("total_releases")).toInt(0);

	const QJsonArray release_array = root.value(QStringLiteral("releases")).toArray();
	for (const QJsonValue &value : release_array) {
		const DiscogsRelease release = releaseFromJson(value.toObject());
		if (!release.title.isEmpty() || !release.artists.isEmpty()) {
			releases.push_back(release);
		}
	}

	return !releases.isEmpty();
}

bool saveSquareArtwork(const QByteArray &bytes, const QString &path, QString &error)
{
	QImage image;
	if (!image.loadFromData(bytes)) {
		error = QStringLiteral("Discogs artwork could not be decoded.");
		return false;
	}

	const int side = qMax(image.width(), image.height());
	if (side <= 0) {
		error = QStringLiteral("Discogs artwork had invalid dimensions.");
		return false;
	}

	QImage square(side, side, QImage::Format_ARGB32_Premultiplied);
	square.fill(Qt::transparent);

	QPainter painter(&square);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

	const int x = (side - image.width()) / 2;
	const int y = (side - image.height()) / 2;
	painter.drawImage(x, y, image);
	painter.end();

	if (!square.save(path, "PNG")) {
		error = QStringLiteral("Unable to write square artwork cache file.");
		return false;
	}

	return true;
}

QString percentEncodePathSegment(const QString &value)
{
	return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

struct HttpResponse {
	CURLcode curl_code = CURLE_OK;
	long http_status = 0;
	QByteArray body;
	QString error;
};

size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	const size_t total = size * nmemb;
	auto *body = static_cast<QByteArray *>(userdata);
	body->append(ptr, static_cast<int>(total));
	return total;
}

QString discogsAuthorizationHeader(const QString &token)
{
	return QStringLiteral("Authorization: Discogs token=") + token;
}

HttpResponse httpGet(const QString &url, const QString &token, const QByteArray &acceptHeader)
{
	static std::once_flag curl_init_once;
	std::call_once(curl_init_once, [] {
		curl_global_init(CURL_GLOBAL_DEFAULT);
	});

	HttpResponse response;
	CURL *curl = curl_easy_init();
	if (!curl) {
		response.error = QStringLiteral("Unable to initialize curl.");
		return response;
	}

	const QByteArray url_bytes = url.toUtf8();
	const QByteArray auth_bytes = discogsAuthorizationHeader(token).toUtf8();
	const QByteArray user_agent_bytes = QByteArray("User-Agent: ") + QByteArray(kDiscogsUserAgent);
	QByteArray accept_bytes = QByteArray("Accept: ");
	accept_bytes.append(acceptHeader);
	QByteArray error_buffer(CURL_ERROR_SIZE, '\0');

	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, auth_bytes.constData());
	headers = curl_slist_append(headers, user_agent_bytes.constData());
	headers = curl_slist_append(headers, accept_bytes.constData());

	curl_easy_setopt(curl, CURLOPT_URL, url_bytes.constData());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer.data());

	response.curl_code = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.http_status);

	if (response.curl_code != CURLE_OK) {
		response.error = QString::fromUtf8(error_buffer.constData());
		if (response.error.isEmpty()) {
			response.error = QString::fromUtf8(curl_easy_strerror(response.curl_code));
		}
	} else if (response.http_status >= 400) {
		response.error = QStringLiteral("HTTP ") + QString::number(response.http_status);
		if (!response.body.isEmpty()) {
			response.error += QStringLiteral(": ") + QString::fromUtf8(response.body.left(240));
		}
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
}

HttpResponse httpGetJson(const QString &url, const QString &token)
{
	return httpGet(url, token, QByteArray("application/vnd.discogs.v2+json"));
}

HttpResponse httpGetBinary(const QString &url, const QString &token)
{
	return httpGet(url, token, QByteArray("application/octet-stream"));
}

struct DiscogsSyncResult {
	bool success = false;
	QString error;
	QString resolved_username;
	int total_releases = 0;
	QVector<DiscogsRelease> releases;
};

QString discogsCollectionUrl(const QString &username, int page)
{
	return QString::fromUtf8(kDiscogsBaseUrl)
		+ QStringLiteral("/users/")
		+ percentEncodePathSegment(username)
		+ QStringLiteral("/collection/folders/0/releases?page=")
		+ QString::number(page)
		+ QStringLiteral("&per_page=100");
}

QString discogsIdentityUrl()
{
	return QString::fromUtf8(kDiscogsBaseUrl) + QStringLiteral("/oauth/identity");
}

DiscogsRelease parseRelease(const QJsonObject &item)
{
	const QJsonObject basic = item.value("basic_information").toObject();

	DiscogsRelease release;
	release.collection_item_id = item.value("id").toInt();
	release.release_id = basic.value("id").toInt();
	release.title = basic.value("title").toString().trimmed();
	release.artists = joinNames(basic.value("artists").toArray(), "name");
	if (release.artists.isEmpty()) {
		release.artists = basic.value("artists_sort").toString().trimmed();
	}
	release.labels = joinNames(basic.value("labels").toArray(), "name");
	release.year = basic.value("year").toInt();

	const QJsonArray images = basic.value("images").toArray();
	if (!images.isEmpty()) {
		const QJsonObject first = images.first().toObject();
		release.artwork_url = first.value("uri").toString().trimmed();
	}
	if (release.artwork_url.isEmpty()) {
		release.artwork_url = basic.value("cover_image").toString().trimmed();
	}
	if (release.artwork_url.isEmpty()) {
		release.artwork_url = basic.value("thumb").toString().trimmed();
	}
	release.cache_path = releaseCachePath(release.release_id);
	return release;
}

bool appendCollectionPage(const QString &token, const QString &username, int page, QVector<DiscogsRelease> &releases, QString &error, int &pages_out, int &items_out)
{
	const HttpResponse response = httpGetJson(discogsCollectionUrl(username, page), token);
	if (!response.error.isEmpty()) {
		error = response.error;
		return false;
	}

	const QJsonDocument doc = QJsonDocument::fromJson(response.body);
	const QJsonObject obj = doc.object();
	const QJsonObject pagination = obj.value("pagination").toObject();
	pages_out = qMax(1, pagination.value("pages").toInt(1));
	items_out = qMax(items_out, pagination.value("items").toInt(0));

	const QJsonArray items = obj.value("releases").toArray();
	for (const QJsonValue &value : items) {
		const DiscogsRelease release = parseRelease(value.toObject());
		if (!release.title.isEmpty() || !release.artists.isEmpty()) {
			releases.push_back(release);
		}
	}

	return true;
}

DiscogsSyncResult syncDiscogs(const QString &token, const QString &override_username, const QString &known_username, const std::function<void(int, int, int, int)> &progress_callback)
{
	DiscogsSyncResult result;
	QString username = override_username.trimmed();
	if (username.isEmpty()) {
		username = known_username.trimmed();
	}

	if (username.isEmpty()) {
		const HttpResponse identity = httpGetJson(discogsIdentityUrl(), token);
		if (!identity.error.isEmpty()) {
			result.error = QStringLiteral("Discogs identity lookup failed: ") + identity.error;
			return result;
		}

		const QJsonDocument doc = QJsonDocument::fromJson(identity.body);
		username = doc.object().value("username").toString().trimmed();
		if (username.isEmpty()) {
			result.error = QStringLiteral("Discogs identity response did not include a username.");
			return result;
		}
	}

	QVector<DiscogsRelease> releases;
	QString error;
	int pages = 1;
	int total_items = 0;
	for (int page = 1; page <= pages; ++page) {
		if (!appendCollectionPage(token, username, page, releases, error, pages, total_items)) {
			result.error = QStringLiteral("Discogs collection sync failed: ") + error;
			return result;
		}

		if (progress_callback) {
			progress_callback(page, pages, releases.size(), total_items);
		}
	}

	result.success = true;
	result.resolved_username = username;
	result.total_releases = releases.size();
	result.releases = std::move(releases);
	return result;
}

HttpResponse downloadArtwork(const QString &token, const QString &url)
{
	return httpGetBinary(url, token);
}
} // namespace

class VinylReelDock final : public QWidget {
public:
	explicit VinylReelDock(QWidget *parent = nullptr) : QWidget(parent)
	{
		auto *root = new QVBoxLayout(this);
		root->setContentsMargins(12, 12, 12, 12);
		root->setSpacing(10);

		auto *title = new QLabel(tr("Vinyl Reel"), this);
		QFont title_font = title->font();
		title_font.setPointSize(title_font.pointSize() + 2);
		title_font.setBold(true);
		title->setFont(title_font);

		m_status = new QLabel(tr("Connect Discogs, sync your collection, then pick a release."), this);
		m_status->setWordWrap(true);

		m_save_button = new QPushButton(tr("Save"), this);
		m_sync_button = new QPushButton(tr("Sync collection"), this);
		m_render_button = new QPushButton(tr("Render"), this);
		m_render_button->setEnabled(false);

		auto *filter_row = new QHBoxLayout();
		m_filter = new QLineEdit(this);
		m_filter->setPlaceholderText(tr("Filter releases"));
		filter_row->addWidget(new QLabel(tr("Filter"), this));
		filter_row->addWidget(m_filter, 1);

		m_identity = new QLabel(tr("Discogs user: not connected"), this);
		m_identity->setWordWrap(true);
		m_collection_summary = new QLabel(tr("Collection: not synced yet"), this);
		m_collection_summary->setWordWrap(true);

		auto *tabs = new QTabWidget(this);

		auto *settings_page = new QWidget(this);
		auto *settings_layout = new QVBoxLayout(settings_page);
		settings_layout->setContentsMargins(0, 0, 0, 0);
		settings_layout->setSpacing(10);
		auto *form = new QFormLayout();
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		m_token = new QLineEdit(this);
		m_token->setEchoMode(QLineEdit::Password);
		m_token->setPlaceholderText(tr("Discogs personal access token"));
		form->addRow(tr("Token"), m_token);
		m_username = new QLineEdit(this);
		m_username->setPlaceholderText(tr("Username (optional, otherwise auto-detect)"));
		form->addRow(tr("Username"), m_username);
		auto *settings_button_row = new QHBoxLayout();
		settings_button_row->addWidget(m_save_button);
		settings_button_row->addWidget(m_sync_button);
		settings_button_row->addStretch();
		settings_layout->addLayout(form);
		settings_layout->addLayout(settings_button_row);
		settings_layout->addStretch();

		auto *browser_page = new QWidget(this);
		auto *browser_layout = new QVBoxLayout(browser_page);
		browser_layout->setContentsMargins(0, 0, 0, 0);
		browser_layout->setSpacing(10);

		m_release_list = new QListWidget(this);
		m_release_list->setMinimumWidth(0);
		m_release_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		m_release_list->setSelectionMode(QAbstractItemView::SingleSelection);

		auto *preview_panel = new QVBoxLayout();
		m_preview_title = new QLabel(tr("No release selected"), this);
		m_preview_title->setWordWrap(true);
		m_preview = new QLabel(this);
		m_preview->setMinimumSize(0, 0);
		m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		m_preview->setMinimumHeight(120);
		m_preview->setAlignment(Qt::AlignCenter);
		m_preview->setStyleSheet("QLabel { background: #222; border: 1px solid #444; }");
		m_preview->setText(tr("Artwork preview"));
		m_preview->setWordWrap(true);
		m_preview_details = new QLabel(tr("Sync a collection and choose a release."), this);
		m_preview_details->setWordWrap(true);
		preview_panel->addWidget(m_preview_title);
		preview_panel->addWidget(m_preview);
		preview_panel->addWidget(m_preview_details);
		preview_panel->addStretch();

		browser_layout->addWidget(m_status);
		browser_layout->addWidget(m_identity);
		browser_layout->addWidget(m_collection_summary);
		browser_layout->addLayout(filter_row);
		browser_layout->addWidget(m_release_list, 1);
		browser_layout->addLayout(preview_panel);
		browser_layout->addWidget(m_render_button);

		tabs->addTab(browser_page, tr("Browser"));
		tabs->addTab(settings_page, tr("Settings"));

		root->addWidget(title);
		root->addWidget(tabs, 1);

		loadSettings();
		refreshUi();

		connect(m_save_button, &QPushButton::clicked, this, [this] {
			saveSettings();
			setStatus(m_token_value.isEmpty()
				? tr("Settings saved, but the Discogs token is still empty.")
				: tr("Settings saved. You can now sync the collection."));
		});

		connect(m_sync_button, &QPushButton::clicked, this, [this] {
			saveSettings();
			startSync();
		});

		connect(m_render_button, &QPushButton::clicked, this, [this] {
			if (const DiscogsRelease *release = selectedRelease()) {
				renderRelease(*release);
			}
		});

		connect(m_filter, &QLineEdit::textChanged, this, [this] {
			populateReleaseList();
		});

		connect(m_release_list, &QListWidget::currentRowChanged, this, [this](int) {
			updatePreviewFromSelection();
		});
	}

	void updateRecordingStatus(const QString &text)
	{
		setStatus(text);
	}

private:
	QLineEdit *m_token = nullptr;
	QLineEdit *m_username = nullptr;
	QLineEdit *m_filter = nullptr;
	QPushButton *m_save_button = nullptr;
	QPushButton *m_sync_button = nullptr;
	QPushButton *m_render_button = nullptr;
	QLabel *m_status = nullptr;
	QLabel *m_identity = nullptr;
	QLabel *m_collection_summary = nullptr;
	QLabel *m_preview_title = nullptr;
	QLabel *m_preview = nullptr;
	QLabel *m_preview_details = nullptr;
	QListWidget *m_release_list = nullptr;
	QVector<DiscogsRelease> m_releases;
	QString m_token_value;
	QString m_username_override;
	QString m_last_known_username;
	QString m_preview_path;
	bool m_syncing = false;

	void setStatus(const QString &text)
	{
		m_status->setText(text);
	}

	void loadSettings()
	{
		QSettings settings(kSettingsOrg, kSettingsApp);
		m_token_value = settings.value("discogs/token").toString().trimmed();
		m_username_override = settings.value("discogs/username_override").toString().trimmed();
		m_last_known_username = settings.value("discogs/username_last_known").toString().trimmed();

		m_token->setText(m_token_value);
		m_username->setText(m_username_override);
		refreshIdentityLabel();
		loadCachedCollection();
	}

	void saveSettings()
	{
		m_token_value = m_token->text().trimmed();
		m_username_override = m_username->text().trimmed();

		QSettings settings(kSettingsOrg, kSettingsApp);
		settings.setValue("discogs/token", m_token_value);
		settings.setValue("discogs/username_override", m_username_override);
		if (!m_last_known_username.isEmpty()) {
			settings.setValue("discogs/username_last_known", m_last_known_username);
		}

		refreshIdentityLabel();
		refreshUi();
	}

	void refreshUi()
	{
		const bool connected = !m_token_value.isEmpty();
		m_sync_button->setEnabled(!m_syncing && connected);
		m_save_button->setEnabled(!m_syncing);
		m_render_button->setEnabled(!m_syncing && selectedRelease() != nullptr);
		m_username->setEnabled(!m_syncing);
		m_token->setEnabled(!m_syncing);
		m_filter->setEnabled(!m_syncing);
	}

	void refreshIdentityLabel()
	{
		const QString override_name = m_username_override.trimmed();
		if (!override_name.isEmpty()) {
			m_identity->setText(tr("Discogs user: ") + override_name);
			return;
		}

		if (!m_last_known_username.isEmpty()) {
			m_identity->setText(tr("Discogs user: ") + m_last_known_username);
			return;
		}

		m_identity->setText(tr("Discogs user: not connected"));
	}

	void updateCollectionSummary(int downloaded, int total, int page, int pages)
	{
		if (total > 0) {
			m_collection_summary->setText(QStringLiteral("Collection: ") + QString::number(downloaded) + QStringLiteral(" / ") + QString::number(total)
				+ QStringLiteral(" releases downloaded (page ") + QString::number(page) + QStringLiteral(" / ") + QString::number(pages) + QStringLiteral(")"));
			return;
		}

		m_collection_summary->setText(QStringLiteral("Collection: ") + QString::number(downloaded)
			+ QStringLiteral(" releases downloaded (page ") + QString::number(page) + QStringLiteral(" / ") + QString::number(pages) + QStringLiteral(")"));
	}

	void startSync()
	{
		if (m_token_value.isEmpty()) {
			setStatus(tr("Enter a Discogs token before syncing."));
			return;
		}

		m_releases.clear();
		populateReleaseList();
		m_syncing = true;
		refreshUi();
		setStatus(tr("Syncing Discogs collection..."));
		m_collection_summary->setText(tr("Collection: syncing..."));

		const QString token = m_token_value;
		const QString override_name = m_username_override.trimmed();
		const QString known_name = m_last_known_username.trimmed();

		QPointer<VinylReelDock> safe_this(this);
		std::thread([safe_this, token, override_name, known_name] {
			const DiscogsSyncResult result = syncDiscogs(token, override_name, known_name, [safe_this](int page, int pages, int downloaded, int total) {
				if (!safe_this) {
					return;
				}

				QMetaObject::invokeMethod(safe_this, [safe_this, page, pages, downloaded, total] {
					if (!safe_this) {
						return;
					}
					safe_this->updateCollectionSummary(downloaded, total, page, pages);
				}, Qt::QueuedConnection);
			});
			if (!safe_this) {
				return;
			}

			QMetaObject::invokeMethod(safe_this, [safe_this, result = std::move(result)]() mutable {
				if (!safe_this) {
					return;
				}
				safe_this->applySyncResult(std::move(result));
			}, Qt::QueuedConnection);
		}).detach();
	}

	void applySyncResult(DiscogsSyncResult result)
	{
		m_syncing = false;
		refreshUi();

		if (!result.error.isEmpty()) {
			setStatus(result.error);
			return;
		}

		if (!result.resolved_username.isEmpty()) {
			m_last_known_username = result.resolved_username;
			saveKnownUsername();
			refreshIdentityLabel();
		}

		m_releases = std::move(result.releases);
		populateReleaseList();
		m_collection_summary->setText(QStringLiteral("Collection: ") + QString::number(result.total_releases) + QStringLiteral(" releases downloaded"));
		saveCollectionSnapshot();

		if (m_releases.isEmpty()) {
			setStatus(tr("Discogs collection sync finished, but no releases were returned."));
			return;
		}

		m_release_list->setCurrentRow(0);
		setStatus(QStringLiteral("Discogs collection synced: ") + QString::number(m_releases.size()) + QStringLiteral(" releases"));
	}

	void saveKnownUsername()
	{
		QSettings settings(kSettingsOrg, kSettingsApp);
		settings.setValue("discogs/username_last_known", m_last_known_username);
	}

	void saveCollectionSnapshot()
	{
		const QString cache_username = !m_username_override.trimmed().isEmpty()
			? m_username_override.trimmed()
			: m_last_known_username.trimmed();
		if (cache_username.isEmpty() || m_releases.isEmpty()) {
			return;
		}

		QString error;
		if (!saveCollectionCache(cache_username, m_releases, error)) {
			setStatus(QStringLiteral("Collection cache save failed: ") + error);
		}
	}

	void loadCachedCollection()
	{
		const QString cache_username = !m_username_override.trimmed().isEmpty()
			? m_username_override.trimmed()
			: m_last_known_username.trimmed();
		if (cache_username.isEmpty()) {
			return;
		}

		QVector<DiscogsRelease> cached_releases;
		int cached_total = 0;
		QString error;
		if (!loadCollectionCache(cache_username, cached_releases, cached_total, error)) {
			return;
		}

		m_releases = std::move(cached_releases);
		populateReleaseList();
		const int total = cached_total > 0 ? cached_total : m_releases.size();
		m_collection_summary->setText(QStringLiteral("Collection: ") + QString::number(total) + QStringLiteral(" releases cached"));

		if (!m_releases.isEmpty()) {
			m_release_list->setCurrentRow(0);
			setStatus(QStringLiteral("Loaded cached Discogs collection: ") + QString::number(m_releases.size()) + QStringLiteral(" releases"));
		}
	}

	void populateReleaseList()
	{
		const QString filter = m_filter->text().trimmed().toLower();
		const int selected_release_id = selectedRelease() ? selectedRelease()->release_id : 0;

		m_release_list->blockSignals(true);
		m_release_list->clear();

		int selected_row = -1;
		for (int i = 0; i < m_releases.size(); ++i) {
			const DiscogsRelease &release = m_releases.at(i);
			const QString label = buildReleaseLabel(release);
			if (!filter.isEmpty() && !label.toLower().contains(filter)) {
				continue;
			}

			auto *item = new QListWidgetItem(label.isEmpty() ? tr("Untitled release") : label, m_release_list);
			item->setData(Qt::UserRole, release.release_id);
			item->setToolTip(label);

			if (release.release_id == selected_release_id) {
				selected_row = m_release_list->count() - 1;
			}
		}

		m_release_list->blockSignals(false);
		if (selected_row >= 0) {
			m_release_list->setCurrentRow(selected_row);
		}
		refreshUi();
	}

	const DiscogsRelease *selectedRelease() const
	{
		const QListWidgetItem *item = m_release_list->currentItem();
		if (!item) {
			return nullptr;
		}

		const int release_id = item->data(Qt::UserRole).toInt();
		for (const DiscogsRelease &release : m_releases) {
			if (release.release_id == release_id) {
				return &release;
			}
		}
		return nullptr;
	}

	void updatePreviewFromSelection()
	{
		const DiscogsRelease *release = selectedRelease();
		m_render_button->setEnabled(!m_syncing && release != nullptr);

		if (!release) {
			m_preview_title->setText(tr("No release selected"));
			m_preview_details->setText(tr("Choose a release from your synced collection."));
			m_preview->setText(tr("Artwork preview"));
			m_preview->setPixmap(QPixmap());
			return;
		}

		m_preview_title->setText(buildReleaseLabel(*release));
		m_preview_details->setText(release->labels.isEmpty()
			? tr("Discogs collection item ") + QString::number(release->collection_item_id)
			: tr("Labels: ") + release->labels);
		renderPreview(*release);
	}

	void renderPreview(const DiscogsRelease &release)
	{
		if (release.artwork_url.isEmpty()) {
			m_preview->setText(tr("No artwork URL returned by Discogs."));
			m_preview->setPixmap(QPixmap());
			return;
		}

		const QFileInfo cache_info(release.cache_path);
		if (cache_info.exists()) {
			setPreviewPixmap(release.cache_path);
			return;
		}

		downloadArtworkAsync(release, false);
	}

	void renderRelease(const DiscogsRelease &release)
	{
		if (release.artwork_url.isEmpty()) {
			setStatus(tr("Selected release does not have artwork."));
			return;
		}

		const QFileInfo cache_info(release.cache_path);
		if (cache_info.exists()) {
			updateArtworkInObs(release.cache_path);
			setStatus(tr("Artwork rendered in OBS."));
			return;
		}

		downloadArtworkAsync(release, true);
	}

	void downloadArtworkAsync(const DiscogsRelease release, bool render)
	{
		const QString token = m_token_value;
		const QString path = release.cache_path;
		const QString url = release.artwork_url;

		QPointer<VinylReelDock> safe_this(this);
		std::thread([safe_this, token, path, url, render] {
			const HttpResponse response = downloadArtwork(token, url);
			if (!safe_this) {
				return;
			}

			QMetaObject::invokeMethod(safe_this, [safe_this, path, render, response = std::move(response)]() mutable {
				if (!safe_this) {
					return;
				}
				if (!response.error.isEmpty()) {
					safe_this->setStatus(QStringLiteral("Artwork download failed: ") + response.error);
					return;
				}

				QString image_error;
				if (!saveSquareArtwork(response.body, path, image_error)) {
					safe_this->setStatus(QStringLiteral("Artwork download failed: ") + image_error);
					return;
				}

				safe_this->setPreviewPixmap(path);
				if (render) {
					safe_this->updateArtworkInObs(path);
					safe_this->setStatus(tr("Artwork rendered in OBS."));
				}
			}, Qt::QueuedConnection);
		}).detach();
	}

	void setPreviewPixmap(const QString &path)
	{
		m_preview_path = path;
		QPixmap pixmap(path);
		if (pixmap.isNull()) {
			m_preview->setText(tr("Unable to load artwork preview."));
			m_preview->setPixmap(QPixmap());
			return;
		}

		const QPixmap scaled = pixmap.scaled(m_preview->size() - QSize(kImageMargin, kImageMargin),
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
		m_preview->setText(QString());
		m_preview->setPixmap(scaled);
	}

	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		if (!m_preview_path.isEmpty()) {
			setPreviewPixmap(m_preview_path);
		}
	}

	void updateArtworkInObs(const QString &filePath)
	{
		obs_source_t *existing = obs_get_source_by_name(kArtworkSourceName);
		if (existing) {
			obs_data_t *settings = obs_data_create();
			obs_data_set_string(settings, "file", filePath.toUtf8().constData());
			obs_source_update(existing, settings);
			obs_data_release(settings);
			obs_source_release(existing);
			return;
		}

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "file", filePath.toUtf8().constData());
		obs_source_t *source = obs_source_create("image_source", kArtworkSourceName, settings, nullptr);
		obs_data_release(settings);
		if (!source) {
			setStatus(tr("Unable to create OBS image source."));
			return;
		}

		obs_source_t *scene_source = obs_frontend_get_current_scene();
		if (scene_source) {
			obs_scene_t *scene = obs_scene_from_source(scene_source);
			if (scene) {
				obs_sceneitem_t *item = obs_scene_add(scene, source);
				if (item) {
					obs_sceneitem_set_visible(item, true);
				}
				obs_scene_release(scene);
			}
			obs_source_release(scene_source);
		} else {
			obs_source_release(source);
			setStatus(tr("Open or select a scene in OBS before rendering artwork."));
			return;
		}

		obs_source_release(source);
	}
};

static QWidget *g_dock_widget = nullptr;

static void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!g_dock_widget) {
		return;
	}

	auto *dock = static_cast<VinylReelDock *>(g_dock_widget);
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		dock->updateRecordingStatus(QObject::tr("Recording is active."));
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		dock->updateRecordingStatus(QObject::tr("Recording stopped."));
		break;
	default:
		break;
	}
}

MODULE_EXPORT bool obs_module_load(void)
{
	blog(LOG_INFO, "vinyl-reel plugin loaded");

	auto *dock = new VinylReelDock();
	g_dock_widget = dock;

	if (!obs_frontend_add_dock_by_id(kDockId, "Vinyl Reel", dock)) {
		blog(LOG_WARNING, "vinyl-reel dock already registered");
		delete dock;
		g_dock_widget = nullptr;
		return false;
	}

	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
	return true;
}

MODULE_EXPORT void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
	obs_frontend_remove_dock(kDockId);
	g_dock_widget = nullptr;
	blog(LOG_INFO, "vinyl-reel plugin unloaded");
}
