#include "pch.h"
#include "CSound.h"

size_t CSound::m_StartIndex = 0;

CSound::CSound()
    : m_index(m_StartIndex++)
{
}

CSound::~CSound()
{
    wchar_t buffer[256];
    swprintf_s(buffer, TEXT("close Sound_%zd"), m_index);
    mciSendString(buffer, nullptr, 0, nullptr);
}

void CSound::Load(const std::wstring& path_)
{
    SetRelativePath(path_);

    wchar_t buffer[256];
    swprintf_s(buffer, L"open \"%s\" alias Sound_%zd", path_.c_str(), m_index);
    MCIERROR err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
    swprintf_s(buffer, L"play Sound_%zd from 0", m_index);
    err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
    swprintf_s(buffer, L"pause Sound_%zd", m_index);
    err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
}

void CSound::Play(bool isLoop_)
{
    wchar_t buffer[256];
    swprintf_s(buffer, L"play Sound_%zd from 0%s", m_index, isLoop_ ? L" repeat" : L"");
    MCIERROR err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
}

void CSound::Pause()
{
    wchar_t buffer[256];
    swprintf_s(buffer, L"pause Sound_%zd", m_index);
    MCIERROR err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
}

void CSound::Stop()
{
    wchar_t buffer[256];
    swprintf_s(buffer, L"stop Sound_%zd", m_index);
    MCIERROR err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
}

float CSound::GetVolume() const
{
    wchar_t buffer[256];
    wchar_t result[256] = { 0 };
    swprintf_s(buffer, L"status Sound_%zd volume", m_index);
    MCIERROR err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
    return _wtof(result) / 1000.0f;
}

void CSound::SetVolume(float volume_)
{
    wchar_t buffer[256];
    int volumeInt = static_cast<int>(volume_ * 1000);
    swprintf_s(buffer, L"setaudio Sound_%zd volume to %d", m_index, volumeInt);
    MCIERROR err = mciSendString(buffer, nullptr, 0, nullptr);
    if (err != 0)
    {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        MessageBox(nullptr, errorText, L"MCI Error", MB_OK);
    }
}
