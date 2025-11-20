# Script PowerShell pour convertir Markdown vers HTML stylisé
$markdown = Get-Content -Path "LNMarkets_Touch_Marketing_Pitch.md" -Raw

# Split into lines for better processing
$lines = $markdown -split "`n"

# Initialize HTML with professional styling
$html = @"
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LN Markets Touch - Pitch Marketing Professionnel</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 900px;
            margin: 0 auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }

        .container {
            background: white;
            border-radius: 15px;
            padding: 30px;
            margin: 20px 0;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
        }

        h1 {
            color: #2E86AB;
            text-align: center;
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.1);
        }

        h2 {
            color: #A23B72;
            border-bottom: 3px solid #A23B72;
            padding-bottom: 10px;
            margin-top: 40px;
            font-size: 1.8em;
        }

        h3 {
            color: #F18F01;
            margin-top: 30px;
            font-size: 1.4em;
        }

        p {
            margin: 15px 0;
            font-size: 1.1em;
        }

        strong {
            color: #2E86AB;
            font-weight: bold;
        }

        em {
            color: #A23B72;
            font-style: italic;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            margin: 25px 0;
            font-size: 0.9em;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            border-radius: 8px;
            overflow: hidden;
        }

        thead tr {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            text-align: left;
            font-weight: bold;
        }

        th, td {
            padding: 12px 15px;
            border-bottom: 1px solid #ddd;
        }

        tbody tr:nth-of-type(even) {
            background-color: #f8f9fa;
        }

        tbody tr:hover {
            background-color: #e3f2fd;
            transition: background-color 0.3s ease;
        }

        .success { color: #28a745; font-weight: bold; font-size: 1.1em; }
        .danger { color: #dc3545; font-weight: bold; font-size: 1.1em; }
        .warning { color: #ffc107; font-weight: bold; font-size: 1.1em; }
        .gold { color: #ffd700; font-weight: bold; font-size: 1.1em; }

        .emoji { font-size: 1.2em; }

        hr {
            border: none;
            height: 2px;
            background: linear-gradient(90deg, #667eea, #764ba2);
            margin: 40px 0;
        }

        blockquote {
            border-left: 4px solid #A23B72;
            padding-left: 20px;
            margin: 20px 0;
            font-style: italic;
            background: #f8f9fa;
            padding: 15px 20px;
            border-radius: 0 8px 8px 0;
        }

        ul, ol {
            margin: 15px 0;
            padding-left: 30px;
        }

        li {
            margin: 8px 0;
        }

        .highlight {
            background: #fff3cd;
            padding: 15px;
            border-left: 4px solid #ffc107;
            border-radius: 0 8px 8px 0;
            margin: 20px 0;
        }

        @media print {
            body {
                background: white;
                max-width: none;
                margin: 0;
                padding: 20px;
            }

            .container {
                box-shadow: none;
                margin: 0;
                padding: 0;
            }
        }
    </style>
</head>
<body>
    <div class="container">
"@

# Process each line
$tableMode = $false
$tableHeaders = @()
$tableRows = @()
$listMode = $false
$listType = ""

foreach ($line in $lines) {
    $line = $line.Trim()

    if ($line -eq "") {
        if ($tableMode) {
            # End table
            $html += "        </tbody>`n    </table>`n"
            $tableMode = $false
            $tableHeaders = @()
            $tableRows = @()
        }
        if ($listMode) {
            # End list
            $html += "        </$listType>`n"
            $listMode = $false
            $listType = ""
        }
        continue
    }

    # Headers
    if ($line -match '^# (.+)$') {
        $html += "        <h1>$($Matches[1])</h1>`n"
    }
    elseif ($line -match '^## (.+)$') {
        $html += "        <h2>$($Matches[1])</h2>`n"
    }
    elseif ($line -match '^### (.+)$') {
        $html += "        <h3>$($Matches[1])</h3>`n"
    }
    # Tables
    elseif ($line -match '^\|.*\|$') {
        if (!$tableMode) {
            # Start table
            $html += "    <table>`n        <thead>`n            <tr>`n"
            $tableMode = $true
        }

        $cells = $line -split '\|' | Where-Object { $_ -ne "" } | ForEach-Object { $_.Trim() }

        if ($tableHeaders.Count -eq 0) {
            # Header row
            $tableHeaders = $cells
            foreach ($cell in $cells) {
                $html += "                <th>$cell</th>`n"
            }
            $html += "            </tr>`n        </thead>`n        <tbody>`n"
        }
        else {
            # Data row
            $html += "            <tr>`n"
            foreach ($cell in $cells) {
                $html += "                <td>$cell</td>`n"
            }
            $html += "            </tr>`n"
        }
    }
    # Horizontal rules
    elseif ($line -match '^---+$') {
        $html += "        <hr>`n"
    }
    # Blockquotes
    elseif ($line -match '^\*".*"$') {
        $quote = $line -replace '^\*"(.*)"$', '$1'
        $html += "        <blockquote>$quote</blockquote>`n"
    }
    # Lists
    elseif ($line -match '^- (.+)$') {
        if (!$listMode -or $listType -ne "ul") {
            if ($listMode) { $html += "        </$listType>`n" }
            $html += "        <ul>`n"
            $listMode = $true
            $listType = "ul"
        }
        $html += "            <li>$($Matches[1])</li>`n"
    }
    elseif ($line -match '^\d+\. (.+)$') {
        if (!$listMode -or $listType -ne "ol") {
            if ($listMode) { $html += "        </$listType>`n" }
            $html += "        <ol>`n"
            $listMode = $true
            $listType = "ol"
        }
        $html += "            <li>$($Matches[1])</li>`n"
    }
    # Regular paragraphs
    else {
        # Process inline formatting
        $line = $line -replace '\*\*(.+?)\*\*', '<strong>$1</strong>'
        $line = $line -replace '\*(.+?)\*', '<em>$1</em>'
        $line = $line -replace '✅', '<span class="success">✅</span>'
        $line = $line -replace '❌', '<span class="danger">❌</span>'
        $line = $line -replace '⚠️', '<span class="warning">⚠️</span>'
        $line = $line -replace '🏆', '<span class="gold">🏆</span>'

        $html += "        <p>$line</p>`n"
    }
}

# Close any open elements
if ($tableMode) {
    $html += "        </tbody>`n    </table>`n"
}
if ($listMode) {
    $html += "        </$listType>`n"
}

# Close HTML
$html += @"
    </div>
</body>
</html>
"@

# Save the file
$html | Out-File -FilePath "LNMarkets_Touch_Marketing_Pitch_Styled.html" -Encoding UTF8

Write-Host "Fichier HTML stylisé créé : LNMarkets_Touch_Marketing_Pitch_Styled.html"